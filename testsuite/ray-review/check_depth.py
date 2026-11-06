"""Grade retained depth, overlay and byte-order observations against literal pixels."""
import argparse
import json
import math
import struct
from copy import deepcopy
from pathlib import Path, PureWindowsPath

WIDTH = HEIGHT = 32
PIXELS = WIDTH * HEIGHT
MODES = ("unpack", "pack", "preserved")
CASES = ("gl_default", "gl_swap", "cpu_default", "cpu_swap")
def need(condition, message):
    if not condition:
        raise ValueError(message)


def integer(value, lo, hi):
    return type(value) is int and lo <= value <= hi


def same_windows_path(a, b):
    return (type(a) is str and type(b) is str and PureWindowsPath(a).is_absolute()
            and PureWindowsPath(b).is_absolute() and PureWindowsPath(a) == PureWindowsPath(b))

def cases(mode):
    return ("gl_prefilled", "cpu_prefilled") if mode == "preserved" else CASES


def filenames(mode, observations):
    """Expected names and sizes. Never consume filenames supplied by the fixture."""
    result = {}
    for name, frame in zip(cases(mode), observations["frames"], strict=True):
        planes = ["actual", "reported", "prefill" if mode == "preserved" else "overlay"]
        if frame.get("cpu_status") == 1:
            planes.append("owned")
            result[f"{name}.owned.coverage.u8"] = PIXELS
        for plane in planes:
            result[f"{name}.{plane}.rgb"] = PIXELS * 3
            result[f"{name}.{plane}.depth.f32"] = PIXELS * 4
    return result


def validate_observations(mode, stage, observations, binding):
    need(observations.get("schema") == "coin.depth-state.observations.v1"
         and observations.get("mode") == mode, f"{mode}: observation identity")
    need(integer(observations.get("width"), WIDTH, WIDTH)
         and integer(observations.get("height"), HEIGHT, HEIGHT), f"{mode}: fixed fixture dimensions")
    need(same_windows_path(observations.get("loaded_coin_module"), binding["coin_module_path"]),
         f"{mode}: actual loaded Coin module path")
    need(all(type(observations.get(key)) is str and 0 < len(observations[key]) <= 1024
             for key in ("gl_vendor", "gl_renderer", "gl_version")), f"{mode}: GL driver identity")
    fbo = observations.get("framebuffer")
    need(integer(fbo, 1, 2**31 - 1) and integer(observations.get("depth_bits"), 16, 64)
         and integer(observations.get("samples"), 0, 0), f"{mode}: private single-sample depth target")
    cleanup = observations["cleanup"]
    need(type(cleanup) is dict
         and all(cleanup.get(key) is True for key in ("restored_context", "context_deleted", "dc_released",
                                                     "window_gone", "class_unregistered"))
         and cleanup.get("gl_errors") == [], f"{mode}: native context and window cleanup")
    rows = observations["frames"]
    need(type(rows) is list and len(rows) == len(cases(mode)), f"{mode}: frame count")
    previous_serial = 0
    for name, frame in zip(cases(mode), rows, strict=True):
        need(type(frame) is dict, f"{name}: frame observation object")
        cpu = name.startswith("cpu_")
        clear = mode != "preserved"
        requested_swap = name.endswith("_swap")
        fallback = cpu and stage == "repair" and mode == "preserved"
        expected_status = 2 if fallback else int(cpu)
        need(frame.get("case") == name and frame.get("cpu_requested") is cpu
             and frame.get("clear_depth") is clear and frame.get("clear_color") is clear,
             f"{name}: fixed request")
        need(frame.get("rendered") is True and integer(frame.get("requested_pipeline"), 1, 1)
             and integer(frame.get("used_pipeline"), 1, 1) and integer(frame.get("fallback_reason"), 0, 0)
             and integer(frame.get("cpu_status"), expected_status, expected_status), f"{name}: actual draw route")
        need(type(frame.get("cpu_detail")) is str and len(frame["cpu_detail"]) <= 8192,
             f"{name}: diagnostic bound")
        if fallback:
            detail = frame["cpu_detail"].lower()
            need("depth" in detail and "clear" in detail, f"{name}: explicit preserved-depth fallback reason")
        need(integer(frame.get("frame_serial"), previous_serial + 1, 2**64 - 1)
             and integer(frame.get("cpu_framebuffer"), fbo, fbo)
             and integer(frame.get("cpu_width"), WIDTH, WIDTH) and integer(frame.get("cpu_height"), HEIGHT, HEIGHT)
             and frame.get("readback_valid") is True, f"{name}: native frame/target/readback identity")
        previous_serial = frame["frame_serial"]
        for transfer in ("pack", "unpack"):
            expected_swap = mode == transfer and requested_swap
            need(frame.get(transfer + "_before") is expected_swap and frame.get(transfer + "_after") is expected_swap,
                 f"{name}: {transfer} swap state and restoration")
        need(frame.get("oracle_pack_swap") is False and frame.get("gl_errors") == [],
             f"{name}: neutral direct readback and GL error state")
        if mode != "preserved":
            need(frame.get("overlay_rendered") is True and integer(frame.get("overlay_used_pipeline"), 1, 1)
                 and integer(frame.get("overlay_fallback_reason"), 0, 0), f"{name}: retained overlay route")
        if expected_status == 1:
            need(integer(frame.get("triangle_count"), 1, 100) and integer(frame.get("hit_count"), 256, 256),
                 f"{name}: CPU geometry/hits")
        else:
            need(integer(frame.get("triangle_count"), 0, 0) and integer(frame.get("hit_count"), 0, 0),
                 f"{name}: no CPU preparation result on retained route")


def literal(x, y, plane):
    inside = 8 <= x < 24 and 8 <= y < 24
    if plane in ("prefill", "preserved") and x < 16:
        return (0, 0, 255), 0.25, False
    if plane != "prefill" and inside:
        if plane == "overlay" and 17 <= x < 23 and 13 <= y < 19:
            return (0, 255, 0), 0.375, True
        return (255, 0, 0), 0.5, True
    return (0, 0, 0), 1.0, False


def samples(raw, name, plane):
    rgb = raw[f"{name}.{plane}.rgb"]
    depth = raw[f"{name}.{plane}.depth.f32"]
    need(len(rgb) == PIXELS * 3 and len(depth) == PIXELS * 4, f"{name}: payload cardinality")
    return rgb, depth


def sample(raw, name, plane, x, y):
    rgb, depth = samples(raw, name, plane)
    index = y * WIDTH + x
    value = struct.unpack_from("<f", depth, index * 4)[0]
    return {"xy": [x, y], "rgb": list(rgb[3 * index:3 * index + 3]),
            "depth": value if math.isfinite(value) else str(value),
            "depth_bytes_le": depth[index * 4:index * 4 + 4].hex()}


def compare_literal(raw, name, plane, expected, tolerance, coverage=None):
    rgb, depth = samples(raw, name, plane)
    rgb_count = depth_count = 0
    first_rgb = first_depth = None
    for index, (actual_z,) in enumerate(struct.iter_unpack("<f", depth)):
        y, x = divmod(index, WIDTH)
        expected_rgb, expected_z, covered = literal(x, y, expected)
        pixel = tuple(rgb[3 * index:3 * index + 3])
        if coverage is not None:
            need(coverage[index] == int(covered), f"{name}: CPU coverage ({x},{y})")
        if any(abs(a - b) > 1 for a, b in zip(pixel, expected_rgb, strict=True)):
            rgb_count += 1
            first_rgb = first_rgb or {"xy": [x, y], "observed": pixel, "expected": expected_rgb}
        if not math.isfinite(actual_z) or not 0 <= actual_z <= 1 or abs(actual_z - expected_z) > tolerance:
            depth_count += 1
            first_depth = first_depth or {"xy": [x, y], "observed": actual_z if math.isfinite(actual_z) else str(actual_z),
                                           "expected": expected_z}
    return {"rgb_mismatches": rgb_count, "depth_mismatches": depth_count,
            "first_rgb_mismatch": first_rgb, "first_depth_mismatch": first_depth}


def pass_pixels(record):
    return record["rgb_mismatches"] == record["depth_mismatches"] == 0


def evaluate(mode, stage, observations, raw, binding):
    validate_observations(mode, stage, observations, binding)
    expected_files = filenames(mode, observations)
    need({name: len(data) for name, data in raw.items()} == expected_files, f"{mode}: complete payload set")
    tolerance = max(3e-6, 4 / (2**observations["depth_bits"] - 1))
    records = {}
    for name in cases(mode):
        expected = "preserved" if mode == "preserved" else "clear"
        records[name] = record = {"actual": compare_literal(raw, name, "actual", expected, tolerance)}
        if mode == "preserved":
            record["prefill"] = compare_literal(raw, name, "prefill", "prefill", tolerance)
            need(pass_pixels(record["prefill"]), f"{name}: independent prefilled sentinel")
        else:
            record["overlay"] = compare_literal(raw, name, "overlay", "overlay", tolerance)
        if name + ".owned.coverage.u8" in raw:
            record["owned"] = compare_literal(raw, name, "owned", "clear", tolerance,
                                               raw[name + ".owned.coverage.u8"])
            need(pass_pixels(record["owned"]), f"{name}: independently expected owned CPU output")
        actual_rgb, actual_depth = samples(raw, name, "actual")
        reported_rgb, reported_depth = samples(raw, name, "reported")
        need(actual_rgb == reported_rgb, f"{name}: reported RGB differs from direct target readback")
        record["witnesses"] = {
            "left_actual": sample(raw, name, "actual", 12, 16),
            "right_actual": sample(raw, name, "actual", 20, 16),
            "right_reported": sample(raw, name, "reported", 20, 16),
        }
        if mode != "preserved":
            record["witnesses"]["right_overlay"] = sample(raw, name, "overlay", 20, 16)
        record["reported_depth_exact"] = reported_depth == actual_depth
        if mode == "pack" and name.endswith("_swap"):
            reversed_depth = b"".join(actual_depth[i:i + 4][::-1] for i in range(0, len(actual_depth), 4))
            record["reported_depth_exact_byte_swap"] = reported_depth == reversed_depth
        else:
            need(record["reported_depth_exact"], f"{name}: reported depth differs from direct target readback")
    if mode != "preserved":
        for name in ("gl_default", "gl_swap", "cpu_default"):
            need(pass_pixels(records[name]["actual"]) and pass_pixels(records[name]["overlay"]),
                 f"{name}: literal render and retained overlay controls")
        for extension in ("rgb", "depth.f32", "coverage.u8"):
            need(raw[f"cpu_default.owned.{extension}"] == raw[f"cpu_swap.owned.{extension}"],
                 "complete owned RGB/depth/coverage must stay byte-identical across swap state")
    else:
        need(pass_pixels(records["gl_prefilled"]["actual"]), "ordinary retained preserved-target control")
        need(samples(raw, "gl_prefilled", "prefill") == samples(raw, "cpu_prefilled", "prefill"),
             "same exact prefill before both preserved-target routes")

    if mode == "unpack":
        changed = records["cpu_swap"]
        depth = changed["witnesses"]["right_actual"]["depth"]
        overlay = changed["witnesses"]["right_overlay"]
        reproduced = (changed["actual"]["depth_mismatches"] > 0 and changed["actual"]["rgb_mismatches"] == 0
                      and type(depth) is float and 0 <= depth <= tolerance
                      and overlay["rgb"] == [255, 0, 0] and overlay["depth"] == depth)
        repaired = pass_pixels(changed["actual"]) and pass_pixels(changed["overlay"])
    elif mode == "pack":
        need(pass_pixels(records["cpu_swap"]["actual"]) and pass_pixels(records["cpu_swap"]["overlay"]),
             "pack swap leaves actual framebuffer and retained overlay correct")
        for prefix in ("cpu", "gl"):
            need(samples(raw, prefix + "_default", "actual") == samples(raw, prefix + "_swap", "actual"),
                 f"{prefix}: exact actual framebuffer equality across pack swap state")
        swapped = [records[name] for name in ("gl_swap", "cpu_swap")]
        reproduced = all(not record["reported_depth_exact"] and record["reported_depth_exact_byte_swap"]
                         for record in swapped)
        repaired = all(record["reported_depth_exact"] for record in swapped)
    else:
        cpu = records["cpu_prefilled"]["actual"]
        rgb, depth = samples(raw, "cpu_prefilled", "actual")
        index = 16 * WIDTH + 12
        red = tuple(rgb[3 * index:3 * index + 3])
        z = struct.unpack_from("<f", depth, index * 4)[0]
        reproduced = (cpu["rgb_mismatches"] > 0 and cpu["depth_mismatches"] > 0
                      and red == (255, 0, 0) and abs(z - 0.5) <= tolerance)
        repaired = (pass_pixels(cpu) and samples(raw, "gl_prefilled", "actual")
                    == samples(raw, "cpu_prefilled", "actual"))
    if stage == "baseline":
        verdict = "PREDICTION_REPRODUCED" if reproduced else "PREDICTION_NOT_REPRODUCED"
    else:
        verdict = "REPAIR_PASS" if repaired else "REPAIR_MISMATCH"
    return {"verdict": verdict, "depth_tolerance": tolerance, "frames": records}


def negative_controls(mode, stage, observations, raw, binding):
    """Mutate copies of real evidence and ensure gates distinguish broken data."""
    name = cases(mode)[0]
    pixel = 16 * WIDTH + 20
    wrong_depth = raw.copy()
    for plane in ("actual", "reported"):
        key = f"{name}.{plane}.depth.f32"
        modified = bytearray(raw[key])
        struct.pack_into("<f", modified, 4 * pixel, 0.875)
        wrong_depth[key] = bytes(modified)
    wrong_state = deepcopy(observations)
    wrong_state["frames"][0]["unpack_after"] = True
    wrong_route = deepcopy(observations)
    wrong_route["frames"][0]["used_pipeline"] = 0
    outcomes = {}
    for label, observed, payload, expected_error in (
        ("corrupted_actual_and_reported_depth", observations, wrong_depth, "control"),
        ("unrestored_unpack_state", wrong_state, raw, "restoration"),
        ("wrong_actual_route", wrong_route, raw, "actual draw route"),
    ):
        try:
            evaluate(mode, stage, observed, payload, binding)
        except ValueError as exc:
            need(expected_error in str(exc), f"{mode}: {label} rejected for unexpected reason: {exc}")
            outcomes[label] = str(exc)
        else:
            raise ValueError(f"{mode}: negative control accepted: {label}")
    return outcomes



if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    parser.add_argument("dll", type=Path)
    parser.add_argument("--stage", choices=("baseline", "repair"), default="repair")
    args = parser.parse_args()
    observed = json.loads((args.directory / "observations.json").read_bytes())
    mode = observed["mode"]
    binding = {"coin_module_path": str(args.dll.resolve(strict=True))}
    raw = {name: (args.directory / name).read_bytes() for name in filenames(mode, observed)}
    result = evaluate(mode, args.stage, observed, raw, binding)
    result["controls"] = negative_controls(mode, args.stage, observed, raw, binding)
    print(json.dumps(result, indent=2))
    need(result["verdict"] in ("PREDICTION_REPRODUCED", "REPAIR_PASS"), "depth regression")
