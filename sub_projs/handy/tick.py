#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
"""
tick.py - 一個簡單的定時器腳本
使用方式: python tick.py 5m
支援的時間單位：s(秒), m(分), h(時)
"""

import sys
import time
from datetime import datetime


def parse_time_interval(time_str):
    """解析時間間隔字符串，返回對應的秒數"""
    if not time_str:
        return None

    # 移除空白
    time_str = time_str.strip().lower()

    # 檢查最後一個字符是否為時間單位
    if time_str[-1] in ["s", "m", "h"]:
        unit = time_str[-1]
        try:
            value = float(time_str[:-1])
        except ValueError:
            return None

        # 轉換為秒
        if unit == "s":
            return value
        elif unit == "m":
            return value * 60
        elif unit == "h":
            return value * 3600
    else:
        # 預設為秒
        try:
            return float(time_str)
        except ValueError:
            return None


def main():
    # 檢查參數
    if len(sys.argv) != 2:
        print("使用方式: python tick.py <時間間隔>")
        print("時間單位: s(秒), m(分), h(時)")
        print("範例: python tick.py 5m")
        print("       python tick.py 30s")
        print("       python tick.py 1h")
        sys.exit(1)

    # 解析時間間隔
    interval = parse_time_interval(sys.argv[1])

    if interval is None or interval <= 0:
        print(f"錯誤：無法解析時間間隔 '{sys.argv[1]}'")
        print("請使用正數，支援的單位：s(秒), m(分), h(時)")
        sys.exit(1)

    print(f"定時器啟動，間隔: {sys.argv[1]} ({interval} 秒)")
    print("按 Ctrl+C 停止")
    print("-" * 40)

    try:
        while True:
            # 獲取並輸出當前時間
            current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            print(current_time)

            # 強制輸出緩衝區（立即顯示）
            sys.stdout.flush()

            # 等待指定的時間間隔
            time.sleep(interval)

    except KeyboardInterrupt:
        print("\n" + "-" * 40)
        print("定時器已停止")


if __name__ == "__main__":
    main()
