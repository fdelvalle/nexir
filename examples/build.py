"""Generate two independent NXG7 games."""
from pathlib import Path
from struct import pack
import sys

TICK, HELD, PRESS, RELEASE, COLLISION = range(5)
VELOCITY, POSITION, GOTO, PLACE_NEAR = range(4)
LEFT, RIGHT, UP, DOWN, SPACE, RESTART = range(6)


def color(value):
    return bytes.fromhex(value)


def entity(scene, shape, x, y, vx, vy, width, height, rgb):
    return pack("<BBhhhhHH", scene, shape, x, y, vx, vy, width, height) + color(rgb) + b"\0"


def rule(scene, event, a, b, action, target, x=0, y=0):
    return pack("<6Bhh", scene, event, a, b, action, target, x, y)


def game(backgrounds, entities, rules):
    return (
        pack("<4sBBHHH", b"NXG7", 0, 0, len(backgrounds), len(entities), len(rules))
        + b"".join(color(bg) + b"\0" for bg in backgrounds)
        + b"".join(entities)
        + b"".join(rules)
    )


def crossing():
    entities = [
        entity(0, 2, 306, 440, 0, 0, 28, 24, "ffdc82"),
        entity(0, 0, 10, 220, 200, 0, 100, 32, "e16858"),
        entity(0, 0, 510, 100, -240, 0, 120, 32, "e16858"),
        entity(0, 1, 0, 20, 0, 0, 640, 16, "88f5a4"),
        entity(0, 0, 639, 220, 0, 0, 1, 32, "e16858"),
        entity(0, 0, 0, 100, 0, 0, 1, 32, "e16858"),
    ]
    rules = [
        rule(0, TICK, 0, 0, VELOCITY, 0),
        rule(0, HELD, LEFT, 0, VELOCITY, 0, -220, -110),
        rule(0, HELD, RIGHT, 0, VELOCITY, 0, 220, -110),
        rule(0, HELD, UP, 0, VELOCITY, 0, 0, -220),
        rule(0, COLLISION, 0, 1, GOTO, 0),
        rule(0, COLLISION, 0, 2, GOTO, 0),
        rule(0, COLLISION, 0, 3, GOTO, 1),
        rule(0, COLLISION, 1, 4, POSITION, 1, -100, 220),
        rule(0, COLLISION, 2, 5, POSITION, 2, 639, 100),
        rule(1, PRESS, SPACE, 0, GOTO, 0),
    ]
    return game(["123b5a", "236847"], entities, rules)


def bounce():
    entities = [
        entity(0, 0, 270, 430, 0, 0, 100, 18, "80ceff"),
        entity(0, 2, 300, 220, 142, 165, 22, 22, "fce769"),
        entity(0, 0, 0, 0, 0, 0, 640, 10, "7389a9"),
        entity(0, 0, 0, 0, 0, 0, 10, 480, "7389a9"),
        entity(0, 0, 630, 0, 0, 0, 10, 480, "7389a9"),
        entity(0, 1, 0, 470, 0, 0, 640, 10, "ef6868"),
    ]
    rules = [
        rule(0, TICK, 0, 0, VELOCITY, 0),
        rule(0, HELD, LEFT, 0, VELOCITY, 0, -330),
        rule(0, HELD, RIGHT, 0, VELOCITY, 0, 330),
        rule(0, COLLISION, 0, 3, POSITION, 0, 10, 430),
        rule(0, COLLISION, 0, 4, POSITION, 0, 530, 430),
        rule(0, COLLISION, 1, 2, VELOCITY, 1, 142, 165),
        rule(0, COLLISION, 1, 3, VELOCITY, 1, 142, -165),
        rule(0, COLLISION, 1, 4, VELOCITY, 1, -142, 165),
        rule(0, COLLISION, 1, 0, VELOCITY, 1, -142, -165),
        rule(0, COLLISION, 1, 5, GOTO, 1),
        rule(1, PRESS, SPACE, 0, GOTO, 0),
    ]
    return game(["192235", "672d44"], entities, rules)


def river_flight():
    # Invisible collision guides use the same color as their background.
    land, water = "173c29", "146aa1"
    entities = [
        entity(0, 0, 145, 0, 0, 0, 350, 480, water),
        entity(0, 0, 145, 0, 0, 0, 8, 480, "d8c37c"),
        entity(0, 0, 487, 0, 0, 0, 8, 480, "d8c37c"),
        entity(0, 2, 305, 385, 0, 0, 30, 37, "e5effc"),
        entity(0, 0, 213, -40, 0, 156, 42, 26, "f97373"),
        entity(0, 0, 399, -210, 0, 188, 42, 26, "f97373"),
        entity(0, 0, 333, -300, 0, 94, 28, 20, "55e69a"),
        entity(0, 0, -12, -24, 0, 0, 5, 12, "fff19a"),
        entity(0, 0, 153, 479, 0, 0, 334, 1, water),
        entity(0, 0, 560, 447, 0, -20, 35, 21, "ffe39a"),
        entity(0, 0, 560, 5, 0, 0, 35, 2, land),
        entity(1, 1, 145, 100, 0, 0, 350, 280, "f97373"),
    ]
    rules = [
        rule(0, PRESS, RESTART, 0, GOTO, 0),
        rule(0, TICK, 0, 0, VELOCITY, 3),
        rule(0, HELD, LEFT, 0, VELOCITY, 3, -275),
        rule(0, HELD, RIGHT, 0, VELOCITY, 3, 275),
        rule(0, PRESS, SPACE, 0, PLACE_NEAR, 7, 3, -12),
        rule(0, PRESS, SPACE, 0, VELOCITY, 7, 0, -420),
    ]
    for bank in (1, 2):
        rules.append(rule(0, COLLISION, 3, bank, GOTO, 1))
    for obstacle, x, y in ((4, 213, -40), (5, 399, -210)):
        rules.extend([
            rule(0, COLLISION, 7, obstacle, POSITION, obstacle, x, y),
            rule(0, COLLISION, 3, obstacle, GOTO, 1),
            rule(0, COLLISION, obstacle, 8, POSITION, obstacle, x, y),
        ])
    rules.extend([
        rule(0, COLLISION, 3, 6, POSITION, 9, 560, 447),
        rule(0, COLLISION, 3, 6, POSITION, 6, 333, -300),
        rule(0, COLLISION, 6, 8, POSITION, 6, 333, -300),
        rule(0, COLLISION, 9, 10, GOTO, 1),
        rule(1, PRESS, RESTART, 0, GOTO, 0),
    ])
    return game([land, "672d44"], entities, rules)


if __name__ == "__main__":
    output = Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=True)
    (output / "crossing.nxg").write_bytes(crossing())
    (output / "bounce.nxg").write_bytes(bounce())
    (output / "river_flight.nxg").write_bytes(river_flight())
