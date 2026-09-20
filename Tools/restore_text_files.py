#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
restore_text_files.py —— 还原 merge_text_files.py 生成的合并文件。

示例
----
python restore_text_files.py merged_text.txt
python restore_text_files.py merged_text.txt -o ./restored
python restore_text_files.py merged_text.txt -o ./restored --skip-existing
python restore_text_files.py merged_text.txt --dry-run

说明
----
* 默认还原到当前目录下的 restored/ 文件夹，可用 -o 指定其它目录。
* 文本文件按记录中的编码、BOM、换行符原样写回；
  base64 记录按原始字节写回，因此可以无损还原。
* 如果合并文件在生成后被编辑/复制工具整体转换过换行符（LF <-> CRLF），
  脚本会按记录里的 length 自动补偿回来（默认 --line-endings auto），
  并在结束时提示有多少个文件做过补偿。

注意
----
合并文件是按字符数定位正文的，最稳妥的用法是：改完正文后原样保存，
不要让编辑器或 git（core.autocrlf）把整份文件的换行符统一改掉。
"""

from __future__ import annotations

import argparse
import base64
import codecs
import sys
from pathlib import Path
from urllib.parse import unquote

FORMAT_HEADER = "# ==== TEXT-MERGE v1 ===="
FILE_MARK = "# ==== FILE ===="
END_MARK = "# ==== END ===="

BOM_BYTES = {
    "utf-8": codecs.BOM_UTF8,
    "utf-16-le": codecs.BOM_UTF16_LE,
    "utf-16-be": codecs.BOM_UTF16_BE,
    "utf-32-le": codecs.BOM_UTF32_LE,
    "utf-32-be": codecs.BOM_UTF32_BE,
}


def read_merged_file(path: Path) -> str:
    """读取合并文件（本身以 UTF-8 保存）。"""
    data = path.read_bytes()
    if data.startswith(codecs.BOM_UTF8):
        return data.decode("utf-8-sig")
    for encoding in ("utf-8", "gb18030"):
        try:
            return data.decode(encoding)
        except UnicodeDecodeError:
            continue
    return data.decode("latin-1")


def read_line(text: str, pos: int):
    """从 pos 处读取一行，返回 (不含换行符的行内容, 下一行起始位置)。"""
    newline = text.find("\n", pos)
    if newline < 0:
        return text[pos:], len(text)
    return text[pos:newline], newline + 1


def skip_line_break(text: str, pos: int) -> int:
    """跳过 pos 处的一个换行（\\n 或 \\r\\n）。"""
    if text.startswith("\r\n", pos):
        return pos + 2
    if text.startswith(("\n", "\r"), pos):
        return pos + 1
    return pos


def strip_trailing_break(payload: str) -> str:
    """去掉末尾的一个换行。"""
    for br in ("\r\n", "\n", "\r"):
        if payload.endswith(br):
            return payload[:-len(br)]
    return payload


def compensate_newlines(payload: str, length: int, mode: str = "auto"):
    """把正文长度补回 length，用于抵消整个合并文件被换行符转换（LF <-> CRLF）的影响。

    返回 (调整后的正文, 是否做过调整)。
    """
    if len(payload) == length:
        return payload, False
    if mode == "keep":
        raise ValueError(f"正文长度 {len(payload)} 与记录的 {length} 不一致")

    delta = len(payload) - length

    # 变长了：说明 LF 被换成了 CRLF，每行多了一个 \r
    if delta > 0 and mode in ("auto", "lf"):
        crlf = payload.count("\r\n")
        if delta > crlf:
            raise ValueError(f"正文长度 {len(payload)} 比记录的 {length} 多 {delta}，但只有 {crlf} 个 CRLF")
        if mode == "lf" or delta == crlf:
            return payload.replace("\r\n", "\n"), True
        # auto：只把前 delta 个 CRLF 还原成 LF，其余保留（正文原本混用换行符的情况）
        pieces = payload.split("\r\n")
        out = []
        drop = delta
        for i, piece in enumerate(pieces):
            out.append(piece)
            if i < len(pieces) - 1:
                if drop > 0:
                    out.append("\n")
                    drop -= 1
                else:
                    out.append("\r\n")
        return "".join(out), True

    # 变短了：说明 CRLF 被换成了 LF
    if delta < 0 and mode in ("auto", "crlf"):
        lone_lf = payload.count("\n") - payload.count("\r\n")
        if lone_lf == -delta:
            return payload.replace("\n", "\r\n"), True

    raise ValueError(f"正文长度 {len(payload)} 与记录的 {length} 不一致，无法自动补偿")


def take_text_payload(text: str, pos: int, length: int, ends_with_newline: str, eol_mode: str = "auto"):
    """取出文本记录的正文，返回 (正文, 新位置, 是否补偿过换行符)。

    正常情况直接按记录里的 length 精确截取；如果长度对不上（通常是合并文件在编辑/
    复制过程中整个被转成 CRLF，或反之），则先按结束标记行扫描出正文，再利用 length
    把换行符还原回去。
    """
    payload = text[pos:pos + length]
    tail_pos = pos + length
    tail, _ = read_line(text, tail_pos)
    if len(payload) == length and tail.rstrip("\r") == END_MARK:
        if ends_with_newline == "0":
            tail_pos = skip_line_break(text, tail_pos)   # 跳过合并时补上的那个换行
        return payload, tail_pos, False

    # 按结束标记行扫描正文
    scan = pos
    content_end = -1
    while scan < len(text):
        line_start = scan
        line, scan = read_line(text, scan)
        if line.rstrip("\r") == END_MARK:
            content_end = line_start
            break
    if content_end < 0:
        raise ValueError("找不到结束标记（文件可能被截断或标记行被改过）")

    payload = text[pos:content_end]
    if ends_with_newline == "0":
        payload = strip_trailing_break(payload)
    payload, adjusted = compensate_newlines(payload, length, eol_mode)
    # 返回结束标记行的起始位置，交给调用方统一校验
    return payload, content_end, adjusted


def parse_record(text: str, pos: int, eol_mode: str = "auto"):
    """解析一条记录，返回 (元信息 dict, 内容 str 或 bytes, 新位置, 是否补偿过换行符)。"""
    meta = {"mode": "text"}

    # ---- 读取记录头 ----
    while True:
        line, pos = read_line(text, pos)
        line = line.rstrip("\r")
        if line == "" or line == END_MARK:
            raise ValueError("文件记录头不完整（缺少 path/encoding/length 等字段）")
        if not line.startswith("# ") or "=" not in line:
            raise ValueError(f"无法解析的记录头：{line!r}")
        key, _, value = line[2:].partition("=")
        key = key.strip()
        meta[key] = value.strip()

        if meta.get("mode") == "binary" and key == "size":
            break
        if meta.get("mode") != "binary" and key == "ends_with_newline":
            break

    if "path" not in meta:
        raise ValueError("记录缺少 path 字段")

    # ---- 读取内容 ----
    adjusted = False
    if meta.get("mode") == "binary":
        data_line, pos = read_line(text, pos)
        payload = base64.b64decode(data_line.strip() or b"")
        expected = int(meta.get("size", "0"))
        if len(payload) != expected:
            raise ValueError(
                f"{meta['path']}：base64 解码后长度 {len(payload)} 与记录的 {expected} 不一致"
            )
    else:
        try:
            payload, pos, adjusted = take_text_payload(
                text, pos,
                int(meta.get("length", "0")),
                meta.get("ends_with_newline", "1"),
                eol_mode,
            )
        except ValueError as exc:
            raise ValueError(f"{meta['path']}：{exc}") from None

    # ---- 校验结束标记 ----
    end_line, pos = read_line(text, pos)
    if end_line.rstrip("\r") != END_MARK:
        raise ValueError(f"{meta['path']}：之后缺少结束标记 {END_MARK!r}")

    return meta, payload, pos, adjusted


def iter_records(text: str, eol_mode: str = "auto"):
    """逐个产出 (元信息, 内容, 是否补偿过换行符)。"""
    pos = 0
    total = len(text)
    while pos < total:
        line, pos = read_line(text, pos)
        if line.rstrip("\r") == FILE_MARK:
            meta, payload, pos, adjusted = parse_record(text, pos, eol_mode)
            yield meta, payload, adjusted


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="还原 merge_text_files.py 生成的合并文本文件",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("merged_file", help="合并后的文本文件路径")
    parser.add_argument("-o", "--out-dir", default="restored",
                        help="还原到的目录（默认：当前目录下的 restored）")
    parser.add_argument("--skip-existing", action="store_true",
                        help="已存在的文件不覆盖")
    parser.add_argument("--dry-run", action="store_true",
                        help="只列出将要还原的文件，不实际写入")
    parser.add_argument("--line-endings", choices=("auto", "lf", "crlf", "keep"), default="auto",
                        help="合并文件被换行符转换过时的补偿方式：auto(默认，自动判断)、"
                             "lf(统一还原成 LF)、crlf(统一还原成 CRLF)、keep(不补偿，长度不符就报错)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="输出每个文件的还原情况")
    return parser


def main(argv=None) -> int:
    args = build_parser().parse_args(argv)

    merged_path = Path(args.merged_file).expanduser().resolve()
    if not merged_path.is_file():
        print(f"[错误] 找不到文件：{merged_path}", file=sys.stderr)
        return 2

    out_dir = Path(args.out_dir).expanduser()
    out_root = out_dir.resolve() if out_dir.is_absolute() else (Path.cwd() / out_dir).resolve()

    text = read_merged_file(merged_path)
    if FORMAT_HEADER not in text[:4096]:
        print(f"[警告] 未在前几行找到格式标记 {FORMAT_HEADER!r}，仍尝试解析……")

    crlf = text.count("\r\n")
    if crlf and text.count("\n") == crlf:
        print("[提示] 合并文件的换行符全是 CRLF（生成时用的是 LF），"
              "说明它被编辑/复制工具转换过；下面会自动补偿回原来的换行符。")

    restored = 0
    skipped = 0
    failed = 0
    adjusted = []

    try:
        for meta, payload, was_adjusted in iter_records(text, args.line_endings):
            rel = unquote(meta["path"]).replace("\\", "/")
            target = (out_root / rel).resolve()

            # 安全检查：不允许写到目标目录之外
            try:
                target.relative_to(out_root)
            except ValueError:
                print(f"[跳过] 路径越界：{rel}")
                failed += 1
                continue

            if args.skip_existing and target.exists():
                skipped += 1
                continue

            if was_adjusted:
                adjusted.append(rel)

            if meta.get("mode") == "binary":
                data = payload if isinstance(payload, bytes) else payload.encode("latin-1")
            else:
                encoding = meta.get("encoding", "utf-8")
                try:
                    data = payload.encode(encoding)
                except (LookupError, UnicodeEncodeError) as exc:
                    print(f"[错误] {rel}：按 {encoding} 回写失败（{exc}）")
                    failed += 1
                    continue
                if meta.get("bom", "0") == "1":
                    data = BOM_BYTES.get(encoding, b"") + data

            if args.dry_run:
                print(f"[预览] {rel}（{len(data)} 字节）")
                restored += 1
                continue

            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            restored += 1
            if args.verbose:
                note = "（换行符已补偿）" if was_adjusted else ""
                print(f"[还原] {rel}（{len(data)} 字节）{note}")
    except ValueError as exc:
        print(f"[错误] 解析失败：{exc}", file=sys.stderr)
        return 3

    action = "将要还原" if args.dry_run else "已还原"
    print(f"{action} {restored} 个文件 -> {out_root}")
    if adjusted:
        print(f"其中 {len(adjusted)} 个文件的换行符被补偿还原（合并文件被整体转换过换行符）")
    if skipped:
        print(f"跳过 {skipped} 个已存在的文件")
    if failed:
        print(f"失败 {failed} 个文件")

    return 0 if restored or not failed else 1


if __name__ == "__main__":
    sys.exit(main())
