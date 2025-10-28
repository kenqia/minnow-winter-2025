import re
from datetime import datetime
import matplotlib.pyplot as plt

# 你的 ping 输出文件
DATA_FILE = "data2.txt"

# 正则匹配行中时间戳和 RTT
pattern = re.compile(r'\[([0-9]+\.[0-9]+)\].*time=([\d\.]+)\s*ms')

timestamps = []
rtts = []

with open(DATA_FILE, "r") as f:
    for line in f:
        m = pattern.search(line)
        if m:
            ts = float(m.group(1))            # epoch timestamp
            rtt = float(m.group(2))           # RTT in ms
            timestamps.append(datetime.fromtimestamp(ts))
            rtts.append(rtt)

# 绘图
plt.figure(figsize=(12,4))
plt.plot(timestamps, rtts, marker='.', linestyle='none')
plt.xlabel("Time of day")
plt.ylabel("RTT (ms)")
plt.title("RTT vs Time")
plt.grid(True)
plt.tight_layout()
plt.tight_layout()
plt.savefig("rtt_vs_time.png")
print("图已保存为 rtt_vs_time.png")

