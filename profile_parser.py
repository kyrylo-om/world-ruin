import re
import os

def generate_summary(log_file):
    if not os.path.exists(log_file):
        print(f"Error: Could not find '{log_file}'")
        return

    timing_data = {}
    sys_data = {}
    total_frames = 0

    time_pattern = re.compile(r'\[(.*?)\]\s+([\d\.]+)\s+ms')
    sys_pattern = re.compile(r'\[SYS_(.*?)\]\s+([\d\.]+)')

    with open(log_file, 'r') as f:
        for line in f:
            line = line.strip()
            if line == "--- End of Frame ---":
                total_frames += 1
                continue

            match_time = time_pattern.match(line)
            if match_time:
                scope_name = match_time.group(1)
                time_ms = float(match_time.group(2))
                if scope_name not in timing_data:
                    timing_data[scope_name] = []
                timing_data[scope_name].append(time_ms)
                continue

            match_sys = sys_pattern.match(line)
            if match_sys:
                metric_name = match_sys.group(1)
                val = float(match_sys.group(2))
                if val > 0:
                    if metric_name not in sys_data:
                        sys_data[metric_name] = []
                    sys_data[metric_name].append(val)
                continue

    print(f"\n=== PROFILER TIMING SUMMARY ===")
    print(f"Total Frames Analyzed: {total_frames}")
    print("-" * 85)
    print(f"{'System Name':<25} | {'Calls':<6} | {'Avg (ms)':<9} | {'Min (ms)':<9} | {'Max (ms)':<9} | {'Spike (P99)':<9}")
    print("-" * 85)

    for scope, times in sorted(timing_data.items()):
        times.sort()
        count = len(times)
        avg_time = sum(times) / count
        min_time = times[0]
        max_time = times[-1]

        p99_index = int(count * 0.99) if int(count * 0.99) < count else count - 1
        p99_time = times[p99_index]

        print(f"{scope:<25} | {count:<6} | {avg_time:<9.4f} | {min_time:<9.4f} | {max_time:<9.4f} | {p99_time:<9.4f}")

    print("-" * 85)

    if sys_data:
        print(f"\n=== HARDWARE & RESOURCE USAGE ===")
        print("-" * 65)
        print(f"{'Metric':<15} | {'Average':<10} | {'Minimum':<10} | {'Maximum':<10}")
        print("-" * 65)

        for metric, vals in sorted(sys_data.items()):
            vals.sort()
            avg_val = sum(vals) / len(vals)
            min_val = vals[0]
            max_val = vals[-1]

            suffix = ""
            if metric == "RAM": suffix = " MB"
            if metric == "CPU": suffix = " %"

            print(f"{metric:<15} | {avg_val:<10.2f} | {min_val:<10.2f} | {max_val:<10.2f} {suffix}".strip())
        print("-" * 65)
        print("\n")

if __name__ == "__main__":
    generate_summary("profiler.log")