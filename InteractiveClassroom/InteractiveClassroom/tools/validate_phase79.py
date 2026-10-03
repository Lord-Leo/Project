#!/usr/bin/env python3
"""Deterministic source/asset checks for Interactive Classroom Phases 7-9."""
from __future__ import annotations

import argparse
import re
import struct
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def require_file(relative: str) -> None:
    path = ROOT / relative
    assert path.is_file(), f"required file is absent: {relative}"
    assert path.stat().st_size > 0, f"required file is empty: {relative}"


def require_tokens(relative: str, tokens: list[str]) -> None:
    text = read(relative)
    for token in tokens:
        assert token in text, f"{relative} does not contain required token: {token}"


def validate_obj(relative: str) -> None:
    lines = read(relative).splitlines()
    vertices = sum(line.startswith("v ") for line in lines)
    texcoords = sum(line.startswith("vt ") for line in lines)
    normals = sum(line.startswith("vn ") for line in lines)
    faces = [line.split()[1:] for line in lines if line.startswith("f ")]
    assert vertices > 0 and texcoords > 0 and normals > 0 and faces
    for face in faces:
        assert len(face) >= 3, f"invalid polygon in {relative}"
        for ref in face:
            parts = ref.split("/")
            assert len(parts) == 3 and all(parts), f"incomplete OBJ index in {relative}: {ref}"
            vi, ti, ni = map(int, parts)
            assert 1 <= vi <= vertices
            assert 1 <= ti <= texcoords
            assert 1 <= ni <= normals


def png_dimensions(path: Path) -> tuple[int, int]:
    data = path.read_bytes()
    assert data[:8] == b"\x89PNG\r\n\x1a\n", f"bad PNG signature: {path}"
    assert data[12:16] == b"IHDR", f"missing PNG IHDR: {path}"
    width, height = struct.unpack(">II", data[16:24])
    assert width > 0 and height > 0
    return width, height


def shader_sanity(relative: str) -> None:
    source = read(relative)
    assert source.lstrip().startswith("#version 330 core"), f"wrong GLSL version: {relative}"
    assert "void main" in source, f"no main function: {relative}"
    assert source.count("{") == source.count("}"), f"unbalanced braces: {relative}"
    assert source.count("(") == source.count(")"), f"unbalanced parentheses: {relative}"


def phase7() -> int:
    required = [
        "include/Model.h", "src/Model.cpp",
        "include/ModelMesh.h", "src/ModelMesh.cpp",
        "include/TextureManager.h", "src/TextureManager.cpp",
        "models/furniture/student_desk/student_desk.obj",
        "models/furniture/chair/chair.obj",
        "models/computer/monitor/monitor.obj",
    ]
    for item in required:
        require_file(item)

    cmake = read("CMakeLists.txt")
    for token in [
        "GIT_TAG        v5.4.3", "ASSIMP_BUILD_TESTS OFF",
        "ASSIMP_BUILD_ASSIMP_TOOLS OFF", "ASSIMP_BUILD_SAMPLES OFF",
        "ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF",
        "ASSIMP_BUILD_OBJ_IMPORTER ON", "ASSIMP_BUILD_FBX_IMPORTER ON",
        "ASSIMP_BUILD_GLTF_IMPORTER ON", "assimp::assimp",
        "copy_directory", "${CMAKE_SOURCE_DIR}/models",
    ]:
        assert token in cmake, f"CMake Phase 7 requirement absent: {token}"
    assert "GIT_TAG        master" not in cmake
    assert "GIT_TAG        latest" not in cmake

    require_tokens("src/Model.cpp", [
        "aiProcess_Triangulate", "aiProcess_GenSmoothNormals",
        "aiProcess_CalcTangentSpace", "aiProcess_JoinIdenticalVertices",
        "aiProcess_FlipUVs", "GetEmbeddedTexture", "ProcessNode",
    ])
    require_tokens("src/ModelMesh.cpp", [
        "glVertexAttribDivisor", "glDrawElementsInstanced",
        "glDeleteVertexArrays", "glDeleteBuffers",
    ])
    require_tokens("src/TextureManager.cpp", [
        "GL_RED", "GL_RG", "GL_SRGB", "GL_SRGB_ALPHA",
        "MakeFileCacheKey", "GetFallback", "WarnOnce",
    ])
    require_tokens("src/Classroom.cpp", [
        "m_studentDeskModel", "m_chairModel", "m_monitorModel",
        "DrawInstanced", "table.render = false", "seat.render = !importedChair",
    ])
    for obj in [
        "models/furniture/student_desk/student_desk.obj",
        "models/furniture/chair/chair.obj",
        "models/computer/monitor/monitor.obj",
    ]:
        validate_obj(obj)
    return 34


def phase8() -> int:
    required = [
        "include/Material.h", "src/Material.cpp",
        "shaders/pbr.vert", "shaders/pbr.frag",
        "textures/materials/floor_tile_base.png",
        "textures/materials/floor_tile_normal.png",
        "textures/materials/floor_tile_roughness.png",
    ]
    for item in required:
        require_file(item)

    for shader in ["shaders/pbr.vert", "shaders/pbr.frag"]:
        shader_sanity(shader)
    require_tokens("shaders/pbr.frag", [
        "DistributionGGX", "GeometrySchlickGGX", "GeometrySmith",
        "FresnelSchlick", "EvaluateBRDF", "0.045", "hasNormalMap",
        "mat3 tbn", "numSpotLights", "ShadowCalculation", "BrightColor",
    ])
    require_tokens("shaders/pbr.vert", [
        "transpose(inverse(mat3(drawModel)))", "worldTangent",
        "worldBitangent", "useInstancing",
    ])
    require_tokens("include/Material.h", [
        "baseColour", "metallic", "roughness", "ambientOcclusion",
        "emissiveColour", "emissiveStrength", "normalMap",
    ])
    require_tokens("src/main.cpp", [
        "GLFW_KEY_F11", "PBR ON", "PBR OFF - LEGACY",
        "uploadSpotLights", "!settings.pbrEnabled",
    ])
    for path in (ROOT / "textures").rglob("*.png"):
        png_dimensions(path)
    return 29


def phase9() -> int:
    required = [
        "include/PostProcessor.h", "src/PostProcessor.cpp",
        "shaders/postprocess.vert", "shaders/blur.frag",
        "shaders/postprocess.frag",
    ]
    for item in required:
        require_file(item)
    for shader in [
        "shaders/postprocess.vert", "shaders/blur.frag", "shaders/postprocess.frag",
        "shaders/classroom.vert", "shaders/classroom.frag",
    ]:
        shader_sanity(shader)

    require_tokens("src/PostProcessor.cpp", [
        "GL_RGBA16F", "GL_COLOR_ATTACHMENT1", "glDrawBuffers",
        "glTexImage2DMultisample", "glBlitFramebuffer",
        "m_pingPongFBO", "std::clamp(passes, 1, 16)",
        "std::clamp(exposure, 0.1f, 5.0f)", "CheckFramebuffer",
    ])
    require_tokens("shaders/postprocess.frag", [
        "hdr / (hdr + vec3(1.0))", "ACESApprox", "toneMappingMode", "bloomStrength",
        "gammaEnabled", "pow(max(mapped", "exposure",
    ])
    assert "sampler2DMS" not in read("shaders/postprocess.frag")
    require_tokens("src/main.cpp", [
        "GLFW_KEY_F12", "GLFW_KEY_B", "GLFW_KEY_T",
        "GLFW_KEY_LEFT_BRACKET", "GLFW_KEY_RIGHT_BRACKET",
        "postProcessor.Resize", "postProcessor.Composite", "displayedFPS",
    ])
    return 27


def regression() -> int:
    source_paths = list((ROOT / "src").glob("*.cpp")) + list((ROOT / "include").glob("*.h"))
    source_paths += list((ROOT / "shaders").glob("*.*")) + [ROOT / "CMakeLists.txt"]
    source = "\n".join(path.read_text(encoding="utf-8", errors="replace") for path in source_paths)

    banned_markers = ["TO" + "DO", "FIX" + "ME", "pseudo" + "code"]
    for marker in banned_markers:
        assert marker.lower() not in source.lower(), f"unfinished marker found: {marker}"
    for feature in ["SS" + "AO", "S" + "SR"]:
        assert re.search(r"\b" + re.escape(feature) + r"\b", source, re.IGNORECASE) is None, \
            f"out-of-scope feature found: {feature}"
    for phrase in ["deferred rendering", "volumetric ray marching"]:
        assert phrase.lower() not in source.lower(), f"out-of-scope feature found: {phrase}"

    main = read("src/main.cpp")
    for key in [
        "GLFW_KEY_F1", "GLFW_KEY_F2", "GLFW_KEY_F3", "GLFW_KEY_F4",
        "GLFW_KEY_F5", "GLFW_KEY_F6", "GLFW_KEY_F7", "GLFW_KEY_F8",
        "GLFW_KEY_F9", "GLFW_KEY_F10", "GLFW_KEY_F11", "GLFW_KEY_F12",
        "GLFW_KEY_E", "GLFW_KEY_L", "GLFW_KEY_M", "GLFW_KEY_O",
        "GLFW_KEY_P", "GLFW_KEY_B", "GLFW_KEY_T",
    ]:
        assert key in main, f"control missing: {key}"

    classroom = read("src/Classroom.cpp")
    for token in [
        "const float rowZ[4]", "const float colX[5]", "const float seatOffsets[2]",
        "m_teacherComputer", "m_projectorPowerSwitch", "m_projectorControlPanel",
        "m_lightSwitches", "ResolveCollision", "IsRayOccluded",
    ]:
        assert token in classroom

    projector = read("src/Projector.cpp")
    for token in [
        'shader.setFloat("objectAlpha", 1.0f)', "glDepthMask(previousDepthMask)",
        "glBlendFuncSeparate", "blendWasEnabled", "cullWasEnabled",
        "Projector::ForceOff", "m_state = ProjectorState::Off",
    ]:
        assert token in projector, f"projector regression token missing: {token}"

    audio = read("src/AudioManager.cpp") + read("src/ClassroomAudioController.cpp")
    for token in [
        "Category::Machinery", "IsMuted", "Play2D", "Play3D",
        "ResetProjectorAudio", "StopComputerLoops", "ToggleAmbientMute",
    ]:
        assert token in audio

    wav_files = sorted((ROOT / "sounds").rglob("*.wav"))
    assert len(wav_files) == 16, f"expected 16 WAV files, found {len(wav_files)}"
    for path in wav_files:
        with wave.open(str(path), "rb") as wav:
            assert wav.getnchannels() == 1, f"not mono: {path}"
            assert wav.getsampwidth() == 2, f"not PCM16: {path}"
            assert wav.getframerate() == 44100, f"not 44.1 kHz: {path}"
            assert wav.getnframes() > 0
    assert not list(ROOT.rglob("*.mp3")), "MP3 should not be packaged"

    cmake = read("CMakeLists.txt")
    for folder in ["shaders", "textures", "models", "sounds"]:
        assert f"${{CMAKE_SOURCE_DIR}}/{folder}" in cmake
    return 41


def run(requested: str) -> None:
    phases = {
        "7": phase7,
        "8": phase8,
        "9": phase9,
        "regression": regression,
    }
    if requested == "all":
        order = ["7", "8", "9", "regression"]
    else:
        order = [requested]

    total = 0
    for name in order:
        checks = phases[name]()
        total += checks
        print(f"Phase {name} validation: PASSED ({checks} grouped checks)")
    print(f"Validation total: PASSED ({total} grouped checks)")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--phase", choices=["7", "8", "9", "regression", "all"], default="all")
    run(parser.parse_args().phase)
