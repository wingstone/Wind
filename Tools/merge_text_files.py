#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
merge_text_files.py —— 把一个文件夹下的所有文本文件合并成单个文本文件。

合并后的文件可以用 restore_text_files.py 还原成原来的目录结构，并且做到无损：
文件路径、内容、编码（UTF-8 / GB18030 / UTF-16 等）、BOM、换行符、空文件、
末尾是否有换行，都会被完整保留。

常用示例
--------
python merge_text_files.py .
python merge_text_files.py "E:/Github/Wind/Source" -o wind_source.txt
python merge_text_files.py . --ext py cpp h md ini
python merge_text_files.py . --exclude Content Plugins -o all.txt
python merge_text_files.py . --include-binary -o everything.txt

注意事项
--------
* 合并文件里的 "# ==== ... ====" 标记行不能手工删除或修改。
* 如果要编辑合并文件，请使用不会自动转换换行符的编辑器；
  一旦换行符被批量转换，还原时正文长度会对不上（脚本会报错提示）。
"""

from __future__ import annotations

import argparse
import base64
import codecs
import os
import sys
from pathlib import Path
from urllib.parse import quote

# --------------------------------------------------------------------------
# 合并文件的格式标记
# --------------------------------------------------------------------------
FORMAT_HEADER = "# ==== TEXT-MERGE v1 ===="
FILE_MARK = "# ==== FILE ===="
END_MARK = "# ==== END ===="

# 默认跳过的目录名（在任意层级出现都会跳过）
DEFAULT_EXCLUDE_DIRS = {
    ".git", ".svn", ".hg", ".bzr", ".idea", ".vs", ".vscode",
    "node_modules", "__pycache__", ".mypy_cache", ".pytest_cache",
    ".venv", "venv", "env", "dist", "build", "target", "out",
    ".gradle", ".nuget", ".cache",
    # Unreal Engine 常见的大目录
    "Binaries", "DerivedDataCache", "Intermediate", "Saved",
}

# 默认当作二进制、直接跳过的扩展名（使用 --include-binary 可强制合并）
DEFAULT_BINARY_EXTS = {
    # 图片 / 音频 / 视频
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".ico", ".tga", ".dds", ".exr", ".hdr",
    ".wav", ".mp3", ".ogg", ".flac", ".mp4", ".avi", ".mov", ".mkv", ".webm",
    # 压缩包 / 可执行 / 库
    ".zip", ".7z", ".rar", ".tar", ".gz", ".bz2", ".xz", ".pack",
    ".exe", ".dll", ".pdb", ".lib", ".obj", ".so", ".dylib", ".a", ".wasm",
    # 引擎 / 资源
    ".uasset", ".umap", ".upk", ".udk", ".bin", ".dat", ".pak", ".ubulk", ".uexp",
    # 模型 / 设计 / 字体 / 文档
    ".fbx", ".blend", ".psd", ".sketch", ".ttf", ".otf", ".woff", ".woff2",
    ".pdf", ".doc", ".docx", ".xls", ".xlsx", ".ppt", ".pptx",
    # 其它
    ".pyc", ".pyo", ".class", ".jar",
}

# BOM 与对应的编码（注意先判断 UTF-32，再判断 UTF-16）
_BOMS = (
    (codecs.BOM_UTF8, "utf-8"),
    (codecs.BOM_UTF32_LE, "utf-32-le"),
    (codecs.BOM_UTF32_BE, "utf-32-be"),
    (codecs.BOM_UTF16_LE, "utf-16-le"),
    (codecs.BOM_UTF16_BE, "utf-16-be"),
)

# 依次尝试的编码（不带 BOM 时）
_TEXT_ENCODINGS = ("utf-8", "gb18030", "big5", "shift_jis", "cp1252")


def looks_like_text(text: str) -> bool:
    """粗略判断解码后的字符串是否是文本（控制字符比例不能太高）。"""
    if not text:
        return True
    control = 0
    for ch in text:
        code = ord(ch)
        if code < 32 and ch not in "\t\n\r\f":
            control += 1
    return control / len(text) < 0.10


def decode_text(data: bytes):
    """尝试把字节解码成文本。成功返回 (文本, 编码名, 是否带 BOM)，失败返回 None。"""
    for bom, encoding in _BOMS:
        if data.startswith(bom):
            try:
                text = data[len(bom):].decode(encoding)
            except UnicodeDecodeError:
                return None
            return (text, encoding, True) if looks_like_text(text) else None

    # 含有 NUL 字节的基本上都是二进制文件
    if b"\x00" in data:
        return None

    for encoding in _TEXT_ENCODINGS:
        try:
            text = data.decode(encoding)
        except UnicodeDecodeError:
            continue
        return (text, encoding, False) if looks_like_text(text) else None
    return None


def parse_exts(values):
    """把 --ext py txt .md 之类的参数规整成 {'.py', '.txt', '.md'}。"""
    if not values:
        return None
    result = set()
    for value in values:
        for item in str(value).replace(",", " ").split():
            item = item.strip().lower()
            if not item:
                continue
            result.add(item if item.startswith(".") else "." + item)
    return result or None


def emit_text_record(out, rel_path: str, payload: str, encoding: str, bom: bool) -> None:
    """写入一条文本文件记录。"""
    ends_with_newline = payload.endswith("\n")
    out.write(f"{FILE_MARK}\n")
    out.write(f"# path={quote(rel_path, safe='/')}\n")
    out.write("# mode=text\n")
    out.write(f"# encoding={encoding}\n")
    out.write(f"# bom={1 if bom else 0}\n")
    out.write(f"# length={len(payload)}\n")
    out.write(f"# ends_with_newline={1 if ends_with_newline else 0}\n")
    out.write(payload)
    if not ends_with_newline:
        # 补一个换行，保证结束标记独占一行；还原时会按 length 精确取回正文
        out.write("\n")
    out.write(f"{END_MARK}\n")


def emit_binary_record(out, rel_path: str, data: bytes) -> None:
    """写入一条二进制文件记录（内容是 base64，可无损还原）。"""
    out.write(f"{FILE_MARK}\n")
    out.write(f"# path={quote(rel_path, safe='/')}\n")
    out.write("# mode=binary\n")
    out.write(f"# size={len(data)}\n")
    out.write(base64.b64encode(data).decode("ascii"))
    out.write("\n")
    out.write(f"{END_MARK}\n")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="把文件夹下的所有文本文件合并为一个文本文件（可用 restore_text_files.py 还原）",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("input_dir", nargs="?", default=".", help="要合并的文件夹（默认：当前目录）")
    parser.add_argument("-o", "--output", default="merged_text.txt",
                        help="输出的合并文件（默认：当前目录下的 merged_text.txt）")
    parser.add_argument("--ext", nargs="+", default=None,
                        help="只合并这些扩展名，例如：--ext py txt md ini")
    parser.add_argument("--exclude", nargs="+", default=None,
                        help="额外排除的目录名/文件名")
    parser.add_argument("--no-default-exclude", action="store_true",
                        help="不使用内置的排除目录列表（.git、node_modules 等）")
    parser.add_argument("--include-binary", action="store_true",
                        help="无法识别为文本的文件也一起合并（以 base64 保存，可无损还原）")
    parser.add_argument("--max-size-mb", type=float, default=None,
                        help="跳过大于该大小（MB）的文件")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="输出每个文件的处理情况")
    return parser


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)

    root = Path(args.input_dir).expanduser().resolve()
    if not root.is_dir():
        print(f"[错误] 输入目录不存在或不是文件夹：{root}", file=sys.stderr)
        return 2

    out_path = Path(args.output).expanduser()
    out_path = out_path.resolve() if out_path.is_absolute() else (Path.cwd() / out_path).resolve()

    exclude_dirs = set() if args.no_default_exclude else set(DEFAULT_EXCLUDE_DIRS)
    if args.exclude:
        exclude_dirs.update(args.exclude)
    exts = parse_exts(args.ext)
    max_bytes = None if args.max_size_mb is None else int(args.max_size_mb * 1024 * 1024)

    records = []   # (相对路径, "text"/"binary", 内容, 编码, 是否有 BOM)
    skipped = []   # (相对路径, 跳过原因)

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = sorted(d for d in dirnames if d not in exclude_dirs)
        for name in sorted(filenames):
            path = Path(dirpath) / name
            try:
                if path.resolve() == out_path:      # 不要把自己合并进去
                    continue
            except OSError:
                pass

            rel = path.relative_to(root).as_posix()
            ext = path.suffix.lower()

            if exts is not None and ext not in exts:
                continue
            if exts is None and ext in DEFAULT_BINARY_EXTS and not args.include_binary:
                skipped.append((rel, "二进制扩展名"))
                continue

            try:
                size = path.stat().st_size
            except OSError as exc:
                skipped.append((rel, f"无法访问：{exc}"))
                continue
            if max_bytes is not None and size > max_bytes:
                skipped.append((rel, "超过大小限制"))
                continue

            try:
                data = path.read_bytes()
            except OSError as exc:
                skipped.append((rel, f"读取失败：{exc}"))
                continue

            decoded = decode_text(data)
            if decoded is None:
                if args.include_binary:
                    records.append((rel, "binary", data, "", False))
                    if args.verbose:
                        print(f"[二进制] {rel} ({size} 字节)")
                else:
                    skipped.append((rel, "看起来不是文本文件"))
                continue

            text, encoding, bom = decoded
            records.append((rel, "text", text, encoding, bom))
            if args.verbose:
                print(f"[文本]   {rel} ({encoding}{' + BOM' if bom else ''}, {size} 字节)")

    if not records:
        print("没有找到可以合并的文件。")
        return 1

    text_count = sum(1 for r in records if r[1] == "text")
    binary_count = len(records) - text_count

    out_path.parent.mkdir(parents=True, exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="") as out:
        out.write(f"{FORMAT_HEADER}\n")
        out.write("# 本文件由 merge_text_files.py 生成，请用 restore_text_files.py 还原。\n")
        out.write("# 标记行（# ==== 开头）请勿手工删除或修改。\n")
        out.write(f"# source_root={quote(str(root), safe=':/')}\n")
        out.write(f"# file_count={len(records)}\n")
        out.write(f"# text_file_count={text_count}\n")
        out.write(f"# binary_file_count={binary_count}\n")
        out.write("\n")
        for rel, kind, payload, encoding, bom in records:
            if kind == "text":
                emit_text_record(out, rel, payload, encoding, bom)
            else:
                emit_binary_record(out, rel, payload)
            out.write("\n")

    print(f"合并完成：{len(records)} 个文件（文本 {text_count}，二进制 {binary_count}）")
    print(f"输出文件：{out_path}")

    if skipped:
        print(f"跳过 {len(skipped)} 个文件：")
        for rel, reason in skipped[:20]:
            print(f"  - {rel}（{reason}）")
        if len(skipped) > 20:
            print(f"  ... 其余 {len(skipped) - 20} 个已省略（可用 -v 之外的方式查看）")

    return 0


if __name__ == "__main__":
    sys.exit(main())
