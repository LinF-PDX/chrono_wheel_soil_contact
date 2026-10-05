#!/usr/bin/env python3
"""Check a completed run and its actual mesh, using only Python's standard library."""
import argparse
import math
from pathlib import Path
import re


def verify(log_path, mesh_path):
    log = log_path.read_text()
    labels = ("Time", "Forward travel", "Wheel centre height",
              "Largest rut depth along the passed centreline")
    values = {}
    for label in labels:
        match = re.search(r"^" + re.escape(label) + r":\s+(\S+)", log, re.MULTILINE)
        if not match:
            raise ValueError(f"Missing result: {label}")
        values[label] = float(match[1])
        if not math.isfinite(values[label]):
            raise ValueError(f"Non-finite result: {label}")
    if abs(values["Time"] - 10.0) > 0.002:
        raise ValueError("The ten-second simulation did not finish")
    travel = values["Forward travel"]
    if travel <= 0.5 or values[labels[-1]] <= 0.01:
        raise ValueError("Expected forward travel and a measurable rut")

    vertices, faces = [], []
    for line in mesh_path.read_text().splitlines():
        fields = line.split()
        if fields and fields[0] == "v":
            numbers = tuple(map(float, fields[1:]))
            if len(numbers) < 3 or not all(map(math.isfinite, numbers)):
                raise ValueError("Invalid or non-finite mesh vertex")
            vertices.append(numbers[:3])
        elif fields and fields[0] == "f":
            faces.append(tuple(int(field.split("/")[0]) for field in fields[1:]))
    if not vertices or not faces:
        raise ValueError("Mesh has no vertices or faces")
    if any(len(face) < 3 or any(i < 1 or i > len(vertices) for i in face) for face in faces):
        raise ValueError("Mesh has invalid face indices")

    # The unchanged starter uses X forward and Z up. Inspect passed ground,
    # excluding the wheel's final contact patch, and untouched lateral ground.
    passed_end = -2.0 + travel - 0.45
    track = [(x, z) for x, y, z in vertices if -1.65 <= x <= passed_end and abs(y) < 0.011]
    sides = [z for x, y, z in vertices if abs(y) >= 0.4]
    depressed = [(x, z) for x, z in track if z < -0.01]
    if len(depressed) < 10 or max(x for x, z in depressed) - min(x for x, z in depressed) < 0.5:
        raise ValueError("No extended depressed track behind the wheel")
    if not sides or max(abs(z) for z in sides) > 1e-6:
        raise ValueError("Expected untouched flat soil beside the track")
    print(f"PASS: finite results; time={values['Time']:.4f} s; travel={travel:.4f} m")
    print(f"Mesh: {len(vertices)} vertices, {len(faces)} faces; "
          f"passed centreline depression={-min(z for x, z in track):.6f} m; "
          f"{len(depressed)} depressed centreline vertices; lateral soil stays flat")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("mesh", type=Path, nargs="?", default=Path("output/soil_after.obj"))
    args = parser.parse_args()
    try:
        verify(args.log, args.mesh)
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: {error}\n")
