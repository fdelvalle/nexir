"""Generate two independent NXG7 games."""
from pathlib import Path
from struct import pack
import sys

TICK, HELD, PRESS, RELEASE, COLLISION = range(5)
VELOCITY, POSITION, GOTO = range(3)
LEFT, RIGHT, UP, DOWN, SPACE = range(5)


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


if __name__ == "__main__":
    output = Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=True)
    (output / "crossing.nxg").write_bytes(crossing())
    (output / "bounce.nxg").write_bytes(bounce())
