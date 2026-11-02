#!/usr/bin/env python3
"""Static checks for the three-artifact release contract (requires PyYAML)."""

from pathlib import Path

import yaml


root = Path(__file__).resolve().parents[2]
workflow = yaml.safe_load((root / ".github/workflows/release.yml").read_text())
triggers = workflow.get("on", workflow.get(True))  # PyYAML treats YAML 1.1 'on' as boolean.
assert set(triggers) == {"push"}, "releases must only run on tag pushes"
assert set(triggers["push"]) == {"tags"}, "branch pushes must not publish releases"
assert triggers["push"]["tags"] == ["v*.*.*"]
jobs = workflow["jobs"]
assert set(jobs) == {"cpu_deb", "cuda_deb", "appimage", "release"}
assert set(jobs["release"]["needs"]) == {"cpu_deb", "cuda_deb", "appimage"}


def steps(job):
    return jobs[job]["steps"]


def commands(job):
    return "\n".join(str(step.get("run", "")) for step in steps(job))


def installed_packages(job):
    return set(
        package
        for line in commands(job).splitlines()
        if "sudo apt-get install -y " in line
        for package in line.split("sudo apt-get install -y ", 1)[1].split()
    )


for job in jobs:
    assert jobs[job]["runs-on"] == "ubuntu-22.04", f"{job} must pin the 22.04 ABI baseline"

for job in ("cpu_deb", "cuda_deb"):
    assert {"file", "dpkg-dev"} <= installed_packages(job), f"{job}: missing CPack tools"

assert {"curl", "binutils"} <= installed_packages("appimage")
assert "binutils" in installed_packages("release"), "AppImage verification needs readelf"

for job in ("cpu_deb", "appimage"):
    assert "-DEARSHOT_CUDA=OFF" in commands(job)
    assert "-DGGML_NATIVE=OFF" in commands(job)

# Native=OFF alone enables host-dependent instruction sets in whisper.cpp on
# Ubuntu runners. Check the configure step, not unrelated shell commands.
for job in ("cpu_deb", "cuda_deb", "appimage"):
    configure = [step for step in steps(job) if step.get("name", "").startswith("Configure ")]
    assert len(configure) == 1, f"{job}: expected one configure step"
    options = configure[0]["run"].split()
    for feature in ("SSE42", "AVX", "AVX2", "FMA", "F16C", "BMI2"):
        assert f"-DGGML_{feature}=OFF" in options, f"{job}: GGML_{feature} must be OFF"

cuda = commands("cuda_deb")
assert "developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64" in cuda
assert "cuda-toolkit-12-6" in cuda
assert "-DEARSHOT_CUDA=ON" in cuda
assert "-DGGML_CUDA_NCCL=OFF" in cuda, "release CUDA must not link optional NCCL"
assert "-DGGML_NATIVE=OFF" in cuda
assert "-DCMAKE_CUDA_ARCHITECTURES=" in cuda
assert "86" in cuda
assert "cmake --build build --parallel 1" in cuda

assert 'packaging/appimage/build.sh' in commands("appimage")
assert 'tests/packaging/check-debs.sh' in commands("release")
assert 'tests/packaging/check-appimage.sh' in commands("release")
assert 'chmod +x "$image"' in commands("release")  # artifact download loses executable mode

for job in ("cpu_deb", "cuda_deb", "appimage"):
    uploads = [step for step in steps(job) if step.get("uses", "").startswith("actions/upload-artifact@")]
    assert len(uploads) == 1, job

publish = [step for step in steps("release") if step.get("uses", "").startswith("softprops/action-gh-release@")]
assert len(publish) == 1
assert publish[0]["with"].get("draft") is True, "assets must upload to a draft"
files = publish[0]["with"]["files"].splitlines()
assert len(files) == 3 and len(set(files)) == 3, files
assert all("*" not in file for file in files), files
publish_index = steps("release").index(publish[0])
before_upload = "\n".join(str(step.get("run", "")) for step in steps("release")[:publish_index])
assert 'curl -sS' in before_upload and "-w '%{http_code}'" in before_upload
assert '200)' in before_upload and '404)' in before_upload and "Release is already public" in before_upload
after_upload = steps("release")[publish_index + 1:]
assert len(after_upload) == 1, "publish only after one final verification step"
finalize = after_upload[0]["run"]
assert "gh api" in finalize and "steps.upload.outputs.id" in finalize
assert "releases/$release_id" in finalize
assert ".draft == true" in finalize and "assets" in finalize and "sort_by(.name)" in finalize
assert "stat -c%s" in finalize, "verify uploaded asset sizes"
assert "releases/$release_id" in finalize and "draft=false" in finalize
print("Validated release workflow structure and three-artifact publishing contract")
