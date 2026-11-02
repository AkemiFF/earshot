#!/usr/bin/env python3
"""Static checks for the three-artifact release contract (requires PyYAML)."""

from pathlib import Path

import yaml


root = Path(__file__).resolve().parents[2]
workflow = yaml.safe_load((root / ".github/workflows/release.yml").read_text())
jobs = workflow["jobs"]
assert set(jobs) == {"cpu_deb", "cuda_deb", "appimage", "release"}
assert set(jobs["release"]["needs"]) == {"cpu_deb", "cuda_deb", "appimage"}


def steps(job):
    return jobs[job]["steps"]


def commands(job):
    return "\n".join(str(step.get("run", "")) for step in steps(job))


for job in ("cpu_deb", "cuda_deb", "appimage", "release"):
    assert jobs[job]["runs-on"] == "ubuntu-22.04", job

for job in ("cpu_deb", "appimage"):
    assert "-DEARSHOT_CUDA=OFF" in commands(job)
    assert "-DGGML_NATIVE=OFF" in commands(job)

cuda = commands("cuda_deb")
assert "developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64" in cuda
assert "cuda-toolkit-12-6" in cuda
assert "-DEARSHOT_CUDA=ON" in cuda
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
files = publish[0]["with"]["files"].splitlines()
assert len(files) == 3 and len(set(files)) == 3, files
assert all("*" not in file for file in files), files
print("Validated release workflow structure and three-artifact publishing contract")
