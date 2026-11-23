import base64
import copy
import hashlib
import json
import subprocess
import sys
import unittest

from emitter import ROOT

TOOL = ROOT / "tools/agent"
BUILD = ROOT / "build/core"
CALCULATION = json.loads((TOOL / "calculation.json").read_text(encoding="utf-8"))


def ins(op, *args):
    return {"op": op, "args": list(args)}


def function(params, result, *blocks):
    return {"signature": {"params": params, "result": result}, "blocks": list(blocks)}


def block(params, *instructions):
    return {"params": params, "instructions": list(instructions)}


def expense(panel):
    policy = function([4], "bool", block(
        [4], ins("record.get", 0, 0), ins("record.get", 0, 1), ins("const", 0),
        ins("i64.lt", 3, 1), ins("i64.lt", 2, 1), ins("bool.not", 5),
        ins("bool.and", 4, 6), ins("return", 7)))
    output = [ins("call.cap", 0, 0)] if panel else []
    outcome = block(["bool"], *output, ins("return", 0))
    main = function(["i64", "i64"], "bool", block(
        ["i64", "i64"], ins("record.new", 4, 0, 1), ins("call", 0, 2),
        ins("br.if", 3, 1, 3, 2, 3)), outcome, outcome)
    module = {"types": [{"kind": "record", "entries": ["i64", "i64"]}],
              "constants": [{"type": "i64", "value": "0"}],
              "functions": [policy, main], "exports": [{"name": "main", "function": 1}]}
    if panel:
        module["imports"] = [{"interface": "ui.mock", "operation": "publish_decision", "version": 1,
                              "signature": {"params": ["bool"], "result": "bool"}}]
    return {"module": module}


def envelope(method, request_id, params=None):
    return {"jsonrpc": "2.0", "id": request_id, "method": method, "params": params or {}}


def request(arguments, request_id=3):
    return envelope("tools/call", request_id, {"name": "nexir_emit", "arguments": arguments})


def interact(*messages):
    handshake = envelope("initialize", 1, {"protocolVersion": "2025-06-18",
                         "capabilities": {}, "clientInfo": {"name": "test", "version": "1.0"}})
    notification = {"jsonrpc": "2.0", "method": "notifications/initialized"}
    def wire(message):
        return message if isinstance(message, bytes) else json.dumps(message).encode("ascii")
    payload = b"\n".join(map(wire, (handshake, notification, *messages))) + b"\n"
    run = subprocess.run([sys.executable, str(TOOL / "server.py")], input=payload,
                         capture_output=True, check=True, timeout=15)
    assert not run.stderr, run.stderr
    responses = [json.loads(line) for line in run.stdout.splitlines()]
    assert responses.pop(0)["result"]["protocolVersion"] == "2025-06-18"
    return responses


def content(response):
    result = response["result"]
    payload = json.loads(result["content"][0]["text"])
    assert result["isError"] == (not payload["ok"])
    return payload


class TestEmitter(unittest.TestCase):
    def test_discovery(self):
        listing, = interact(envelope("tools/list", 2))
        tools = listing["result"]["tools"]
        self.assertEqual([tool["name"] for tool in tools], ["nexir_emit"])
        self.assertEqual(tools[0]["inputSchema"]["required"], ["module"])

    def test_all_example_bytes_match_native_emitter(self):
        inputs = [CALCULATION, expense(False), expense(True), CALCULATION]
        results = interact(*(request(args, i + 3) for i, args in enumerate(inputs)))
        for name, response in zip(("calculation", "expense", "panel"), results):
            payload = content(response)
            data = base64.b64decode(payload["bytecode_base64"], validate=True)
            self.assertEqual(data, (BUILD / f"{name}.nxir").read_bytes())
            self.assertEqual(payload["size"], len(data))
            self.assertEqual(payload["sha256"], hashlib.sha256(data).hexdigest())
        self.assertEqual(content(results[0]), content(results[-1]))

    def test_native_verification_and_recovery(self):
        bad = copy.deepcopy(CALCULATION)
        bad["module"]["functions"][0]["blocks"][0]["instructions"][0]["args"] = [99, 1]
        response, next_response = interact(request(bad), request(CALCULATION, 4))
        diagnostic = content(response)["diagnostic"]
        self.assertEqual(diagnostic["path"], "module.functions[0].blocks[0].instructions[0]")
        self.assertEqual(diagnostic["status"], "verification failed")
        self.assertTrue(content(next_response)["ok"])

    def test_exact_i64_and_invalid_literals(self):
        def program(kind, value):
            return {"module": {"constants": [{"type": kind, "value": value}],
                    "functions": [function([], kind, block([], ins("const", 0), ins("return", 0)))],
                    "exports": [{"name": "main", "function": 0}]}}
        valid = program("i64", "-9223372036854775808")
        invalid = [program("i64", "-9223372036854775809"), program("i64", 123),
                   program("text", "\ud800"), program("bytes", "TR==")]
        responses = interact(request(valid), *(request(item, i + 4) for i, item in enumerate(invalid)))
        data = base64.b64decode(content(responses[0])["bytecode_base64"])
        self.assertIn(bytes.fromhex("0000000000000080"), data)
        for response in responses[1:]:
            self.assertEqual(content(response)["diagnostic"]["path"], "module.constants[0].value")

    def test_transport_limits_and_duplicate_keys(self):
        duplicate = b'{"jsonrpc":"2.0","id":2,"method":"tools/list","method":"tools/call"}'
        oversized = b" " * (2 * 1024 * 1024 + 1)
        bad_json, too_big, response = interact(duplicate, oversized, request(CALCULATION))
        self.assertEqual(bad_json["error"]["code"], -32700)
        self.assertEqual(too_big["error"]["code"], -32700)
        self.assertTrue(content(response)["ok"])


if __name__ == "__main__":
    unittest.main()
