"""Structured authoring adapter for the NEXIR IR 1.0 native emitter."""
import base64
import ctypes as c
import hashlib
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRIMITIVES = {"bool": 0, "i64": 1, "text": 2, "bytes": 3}
OPCODES = dict(enumerate((
    "const", "record.new", "record.get", "variant.new", "variant.tag",
    "variant.get", "i64.add", "i64.sub", "i64.mul", "i64.div", "i64.eq",
    "i64.lt", "bool.not", "bool.and", "bool.or", "call", "call.cap", "br",
    "br.if", "return", "trap",
), 1))
OPCODES = {name: code for code, name in OPCODES.items()}
MAX_BINARY = 262144


class AuthoringError(ValueError):
    def __init__(self, path, message):
        super().__init__(message)
        self.path = path


class Diagnostic(c.Structure):
    _fields_ = [("status", c.c_int), ("offset", c.c_size_t), ("message", c.c_char_p)]


def bind_library():
    extension = "dylib" if sys.platform == "darwin" else "so"
    lib = c.CDLL(str(ROOT / f"build/core/libnexir-agent.{extension}"))
    pointer, u16 = c.c_void_p, c.c_uint16
    operands = c.POINTER(u16)
    bindings = {
        "agent_new": (pointer, []),
        "agent_free": (None, [pointer]),
        "agent_type": (c.c_bool, [pointer, u16, operands, u16]),
        "agent_constant": (c.c_bool, [pointer, u16, c.c_char_p, u16]),
        "agent_import": (c.c_bool, [pointer, c.c_char_p, u16, c.c_char_p, u16, operands, u16]),
        "agent_function": (c.c_bool, [pointer, u16, operands, u16]),
        "agent_block": (c.c_bool, [pointer, operands, u16]),
        "agent_instruction": (c.c_bool, [pointer, u16, operands, u16]),
        "agent_export": (c.c_bool, [pointer, c.c_char_p, u16]),
        "nx_emit": (c.c_int, [pointer, pointer, c.c_size_t,
                              c.POINTER(c.c_size_t), c.POINTER(Diagnostic)]),
        "nx_status": (c.c_char_p, [c.c_int]),
    }
    for name, (result, params) in bindings.items():
        function = getattr(lib, name)
        function.restype, function.argtypes = result, params
    return lib


def shape(value, path, required, optional=()):
    if not isinstance(value, dict):
        raise AuthoringError(path, "expected object")
    for key in required:
        if key not in value:
            raise AuthoringError(f"{path}.{key}", "required field")
    extra = value.keys() - set(required) - set(optional)
    if extra:
        raise AuthoringError(path, f"unknown field {min(extra)!r}")
    return value


def array(value, path, maximum):
    if not isinstance(value, list) or len(value) > maximum:
        raise AuthoringError(path, f"expected array with at most {maximum} items")
    return value


def index(value, path, maximum=65535):
    if type(value) is not int or not 0 <= value <= maximum:
        raise AuthoringError(path, f"expected integer in [0, {maximum}]")
    return value


def type_ref(value, path):
    if isinstance(value, str) and value in PRIMITIVES:
        return PRIMITIVES[value]
    return index(value, path, 63)


def version(value, path):
    value = index(value, path)
    if value == 0:
        raise AuthoringError(path, "interface version must be at least 1")
    return value


def sequence(values, path, maximum, convert):
    items = [convert(v, f"{path}[{i}]") for i, v in enumerate(array(values, path, maximum))]
    return (c.c_uint16 * len(items))(*items), len(items)


def name(value, path):
    if not isinstance(value, str) or not re.fullmatch(r"[a-z][a-z0-9._]{0,62}", value):
        raise AuthoringError(path, "expected a NEXIR identifier")
    return value.encode("ascii")


def signature(value, path):
    sig = shape(value, path, ("params", "result"))
    return (type_ref(sig["result"], f"{path}.result"),
            *sequence(sig["params"], f"{path}.params", 8, type_ref))


def literal(value, path):
    row = shape(value, path, ("type", "value"))
    kind, data = row["type"], row["value"]
    field = f"{path}.value"
    if not isinstance(kind, str) or kind not in PRIMITIVES:
        raise AuthoringError(f"{path}.type", "expected primitive literal type")
    if kind == "i64":
        # JSON number implementations may round large integers. Decimal strings preserve every bit.
        if not isinstance(data, str) or not re.fullmatch(r"(?:0|-?[1-9][0-9]{0,18})", data):
            raise AuthoringError(field, "expected canonical signed decimal string")
        number = int(data)
        if not -(1 << 63) <= number < 1 << 63:
            raise AuthoringError(field, "i64 out of range")
        raw = number.to_bytes(8, "little", signed=True)
    elif kind == "bool":
        if type(data) is not bool:
            raise AuthoringError(field, "expected boolean")
        raw = bytes([data])
    elif kind == "text":
        if not isinstance(data, str) or len(data) > 4096:
            raise AuthoringError(field, "expected at most 4096 UTF-8 bytes")
        try:
            raw = data.encode("utf-8", errors="strict")
        except UnicodeError as exc:
            raise AuthoringError(field, "invalid Unicode scalar") from exc
    else:
        if not isinstance(data, str) or len(data) > 5464:
            raise AuthoringError(field, "expected canonical base64")
        try:
            raw = base64.b64decode(data, validate=True)
        except ValueError as exc:
            raise AuthoringError(field, "invalid base64") from exc
        if base64.b64encode(raw).decode("ascii") != data:
            raise AuthoringError(field, "expected canonical base64")
    if len(raw) > 4096:
        raise AuthoringError(field, "literal exceeds 4096 bytes")
    return PRIMITIVES[kind], raw, len(raw)


def emit(lib, document):
    doc = shape(document, "module", ("functions", "exports"), ("types", "constants", "imports"))
    builder = lib.agent_new()
    if not builder:
        raise MemoryError("cannot allocate NEXIR builder")

    def put(operation, path, *args):
        if not operation(builder, *args):
            raise AuthoringError(path, "native builder capacity or table order exceeded")

    try:
        instructions = []
        for i, value in enumerate(array(doc.get("types", []), "module.types", 60)):
            path = f"module.types[{i}]"
            row = shape(value, path, ("kind", "entries"))
            kind = row["kind"]
            if kind == "handle":
                entries = sequence(row["entries"], f"{path}.entries", 1,
                                   lambda v, p: index(v, p, 31))
                put(lib.agent_type, path, 6, *entries)
            elif kind in ("record", "variant"):
                entries = sequence(row["entries"], f"{path}.entries", 8, type_ref)
                put(lib.agent_type, path, 4 if kind == "record" else 5, *entries)
            else:
                raise AuthoringError(f"{path}.kind", "expected record, variant or handle")

        for i, row in enumerate(array(doc.get("constants", []), "module.constants", 128)):
            path = f"module.constants[{i}]"
            put(lib.agent_constant, path, *literal(row, path))

        for i, row in enumerate(array(doc.get("imports", []), "module.imports", 32)):
            path = f"module.imports[{i}]"
            row = shape(row, path, ("interface", "version", "operation", "signature"))
            put(lib.agent_import, path, name(row["interface"], f"{path}.interface"),
                version(row["version"], f"{path}.version"), name(row["operation"], f"{path}.operation"),
                *signature(row["signature"], f"{path}.signature"))

        for i, function in enumerate(array(doc["functions"], "module.functions", 32)):
            function_path = f"module.functions[{i}]"
            function = shape(function, function_path, ("signature", "blocks"))
            put(lib.agent_function, function_path,
                *signature(function["signature"], f"{function_path}.signature"))
            for j, block in enumerate(array(function["blocks"], f"{function_path}.blocks", 16)):
                block_path = f"{function_path}.blocks[{j}]"
                block = shape(block, block_path, ("params", "instructions"))
                put(lib.agent_block, block_path, *sequence(block["params"], f"{block_path}.params", 8, type_ref))
                for k, ins in enumerate(array(block["instructions"], f"{block_path}.instructions", 129)):
                    path = f"{block_path}.instructions[{k}]"
                    ins = shape(ins, path, ("op", "args"))
                    op = ins["op"]
                    if not isinstance(op, str) or op not in OPCODES:
                        raise AuthoringError(f"{path}.op", "unknown NEXIR IR 1.0 opcode")
                    put(lib.agent_instruction, path, OPCODES[op],
                        *sequence(ins["args"], f"{path}.args", 19, index))
                    instructions.append(path)

        for i, row in enumerate(array(doc["exports"], "module.exports", 32)):
            path = f"module.exports[{i}]"
            row = shape(row, path, ("name", "function"))
            put(lib.agent_export, path, name(row["name"], f"{path}.name"),
                index(row["function"], f"{path}.function", 31))

        output = c.create_string_buffer(MAX_BINARY)
        size = c.c_size_t()
        diag = Diagnostic()
        status = lib.nx_emit(builder, output, len(output), c.byref(size), c.byref(diag))
        if status:
            message = diag.message.decode("utf-8")
            path = "module"
            if message == "invalid instruction, operands or SSA reference" and diag.offset < len(instructions):
                path = instructions[diag.offset]
            return {"ok": False, "diagnostic": {"path": path, "message": message,
                    "status": lib.nx_status(status).decode("ascii"), "native_index": diag.offset}}
        data = output.raw[:size.value]
        return {"ok": True, "format": "nexir-ir/1.0", "size": size.value,
                "sha256": hashlib.sha256(data).hexdigest(),
                "bytecode_base64": base64.b64encode(data).decode("ascii")}
    finally:
        lib.agent_free(builder)
