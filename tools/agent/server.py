"""NEXIR emitter server using the MCP stdio transport."""
import json
import sys
from pathlib import Path

from emitter import AuthoringError, bind_library, emit, shape

PROTOCOL = "2025-03-26"
VERSIONS = ("2024-11-05", PROTOCOL, "2025-06-18", "2025-11-25")
MAX_LINE = 2 * 1024 * 1024
TOOL = {
    "name": "nexir_emit",
    "description": (
        "Validate a structured NEXIR IR 1.0 module and return its canonical "
        "binary as base64, byte count and SHA-256. This tool does not run the module."
    ),
    "inputSchema": json.loads(Path(__file__).with_name("schema.json").read_text(encoding="utf-8")),
    "annotations": {"readOnlyHint": True, "idempotentHint": True, "openWorldHint": False},
}


def unique_pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON object key")
        result[key] = value
    return result


def invalid_number(_):
    raise ValueError("non-finite JSON number")


def tool_result(payload):
    return {"content": [{"type": "text", "text": json.dumps(payload, ensure_ascii=True)}],
            "isError": not payload["ok"]}


def error(code, message, request_id=None):
    return {"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}}


def result(request_id, payload):
    return {"jsonrpc": "2.0", "id": request_id, "result": payload}


def handle(lib, request, ready):
    def fail(code, message):
        return error(code, message, request_id), ready

    if not isinstance(request, dict) or request.get("jsonrpc") != "2.0":
        return error(-32600, "Invalid Request"), ready
    method = request.get("method")
    request_id = request.get("id")
    if not isinstance(method, str) or ("id" in request and type(request_id) not in (int, str)):
        return error(-32600, "Invalid Request"), ready
    if "id" not in request:
        return None, ready
    params = request.get("params", {})
    if not isinstance(params, dict):
        return fail(-32602, "Invalid params")
    if method == "ping":
        return result(request_id, {}), ready
    if method == "initialize":
        version = params.get("protocolVersion")
        if not isinstance(version, str):
            return fail(-32602, "Missing protocolVersion")
        response = {"protocolVersion": version if version in VERSIONS else PROTOCOL,
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "nexir-agent-emitter", "version": "0.1.0"}}
        return result(request_id, response), True
    if not ready:
        return fail(-32600, "Initialize the session first")
    if method == "tools/list":
        return result(request_id, {"tools": [TOOL]}), ready
    if method != "tools/call":
        return fail(-32601, "Method not found")
    if params.get("name") != TOOL["name"]:
        return fail(-32602, "Unknown tool")
    try:
        args = shape(params.get("arguments", {}), "arguments", ("module",))
        payload = emit(lib, args["module"])
    except AuthoringError as exc:
        payload = {"ok": False, "diagnostic": {"path": exc.path, "message": str(exc)}}
    except MemoryError:
        return fail(-32603, "Emitter allocation failure")
    return result(request_id, tool_result(payload)), ready


def serve(lib, source, sink):
    ready = False
    while line := source.readline(MAX_LINE + 1):
        if len(line) > MAX_LINE:
            while line and not line.endswith(b"\n"):
                line = source.readline(MAX_LINE + 1)
            response = error(-32700, "Request exceeds 2 MiB")
        else:
            try:
                request = json.loads(line.decode("utf-8"), object_pairs_hook=unique_pairs,
                                     parse_constant=invalid_number)
            except (ValueError, RecursionError):
                response = error(-32700, "Parse error")
            else:
                response, ready = handle(lib, request, ready)
        if response is not None:
            sink.write((json.dumps(response, ensure_ascii=True, separators=(",", ":")) + "\n").encode("ascii"))
            sink.flush()


if __name__ == "__main__":
    serve(bind_library(), sys.stdin.buffer, sys.stdout.buffer)
