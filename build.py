"""Assembler for NEXIR v0.2. Python is used only at build time."""
import struct
from pathlib import Path

ROOT = Path(__file__).parent
OP = {'LOAD': 1, 'INPUT': 2, 'ADD': 3, 'SUB': 4, 'CHECK_POS': 5, 'CHECK_LE': 6, 'STORE': 7, 'STATUS': 8, 'CLEAR': 9, 'PUSH': 10, 'EQ': 11, 'JZ': 12, 'JMP': 13, 'HALT': 255}

def record(kind, payload):
    return struct.pack('<BH', kind, len(payload)) + payload

def widget(kind, ident, x, y, w, h, text=''):
    return record(1, struct.pack('<BBhhhh', kind, ident, x, y, w, h) + text.encode())

def variable(ident, initial):
    return record(3, struct.pack('<Bq', ident, initial))

def action(ident, *instructions):
    code = bytearray()
    for instruction in instructions:
        op, *args = instruction
        code.append(OP[op])
        if op == 'PUSH':
            code.extend(struct.pack('<q', args[0]))
        elif op in ('JZ', 'JMP'):
            code.extend(struct.pack('<h', args[0]))
        else:
            code.extend(args)
    return record(2, struct.pack('<BH', ident, len(code)) + code)

def app(filename, title, width, height, records):
    title = title.encode()
    data = b'NXIR' + struct.pack('<BHHH', 3, width, height, len(title)) + title + struct.pack('<H', len(records)) + b''.join(records)
    (ROOT / filename).write_bytes(data)
    print(filename, len(data), 'bytes')

app('wallet.nx', 'NEXIR Wallet', 460, 340, [
    variable(0, 500000),
    widget(1, 0, 25, 20, 300, 30, 'NEXIR Wallet'),
    widget(4, 0, 25, 70, 360, 30, 'Balance BRL'),
    widget(1, 0, 25, 115, 350, 25, 'Amount in cents:'),
    widget(3, 1, 25, 145, 260, 38),
    widget(2, 2, 25, 205, 260, 45, 'Authorize'),
    widget(5, 0, 25, 280, 400, 30),
    action(2, ('INPUT', 1), ('CHECK_POS',), ('LOAD', 0), ('CHECK_LE',), ('LOAD', 0), ('INPUT', 1), ('SUB',), ('STORE', 0), ('STATUS', 1), ('CLEAR', 1), ('HALT',)),
])
app('calculator.nx', 'NEXIR Calculator', 460, 340, [
    variable(0, 0),
    widget(1, 0, 25, 20, 350, 30, 'NEXIR Calculator'),
    widget(4, 0, 25, 70, 350, 30, 'Result / 100'),
    widget(1, 0, 25, 115, 350, 25, 'Enter a positive integer:'),
    widget(3, 1, 25, 145, 260, 38),
    widget(2, 2, 25, 205, 260, 45, 'Add to result'),
    widget(5, 0, 25, 280, 400, 30),
    action(2, ('LOAD', 0), ('INPUT', 1), ('CHECK_POS',), ('ADD',), ('STORE', 0), ('STATUS', 2), ('CLEAR', 1), ('HALT',)),
])

app('toggle.nx', 'NEXIR Toggle', 460, 280, [
    variable(0, 0),
    widget(1, 0, 25, 20, 350, 30, 'NEXIR Toggle'),
    widget(4, 0, 25, 65, 350, 30, 'State (0 or 1)'),
    widget(2, 2, 25, 115, 260, 45, 'Toggle'),
    widget(5, 0, 25, 195, 400, 30),
    action(2, ('LOAD',0),('PUSH',0),('EQ',),('JZ',14),
           ('PUSH',1),('STORE',0),('STATUS',3),('HALT',),
           ('PUSH',0),('STORE',0),('STATUS',4),('HALT',)),
])
