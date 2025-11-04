# Performance Optimization Guide for Raspberry Pi 5

## Quick Answer: Why NEON Isn't Helping

**NEON is working correctly**, but it's only optimizing a small part of your total processing time. This is a classic example of **Amdahl's Law**.

### The Math

If color accumulation (what NEON optimizes) is only 10% of total processing time:
- Even with 4x speedup from NEON
- Overall improvement = **only ~8%**

Most of your time is likely spent on:
1. **Camera/Image I/O** (40-60%)
2. **Network I/O to HyperHDR** (20-40%)
3. **Color extraction overhead** (10-20%)
   - Only the inner pixel loop is NEON optimized (~5%)

## Step 1: Identify Your Bottleneck

### Run the Profiling Tool

```bash
cd /Users/morteza/Projects/Playground/TV/cpp
./profile_tool.sh
```

This will show you exactly where time is spent:
- Frame I/O time
- Color Extraction time
- Network I/O time
- NEON vs overhead percentage

### Expected Results

On Raspberry Pi 5, typical breakdown:

```
Frame I/O:         25-40 ms (50-60%)
Color Extraction:  3-8 ms  (10-15%)
  - NEON loop:     1-3 ms  (5-8% of total)
  - Overhead:      2-5 ms  (memory access, setup)
Network I/O:       10-20 ms (20-30%)
Other:             2-5 ms   (5-10%)
-------------------
Total:             40-65 ms (15-25 FPS)
```

## Step 2: Optimize the Actual Bottleneck

### If Frame I/O is the Bottleneck (most common)

#### Option 1: Reduce Camera Resolution

Edit `config.json`:
```json
{
  "camera": {
    "enable_scaling": true,
    "scaled_width": 960,
    "scaled_height": 540
  }
}
```

**Expected improvement:** 30-50% faster
**Why:** Less data to transfer from camera, less to process

#### Option 2: Disable Auto-Adjustments

```json
{
  "camera": {
    "autofocus_mode": "manual",
    "lens_position": 0.0,
    "awb_mode": "incandescent",
    "awb_gain_red": 1.5,
    "awb_gain_blue": 1.5
  }
}
```

**Expected improvement:** 10-20% faster
**Why:** Camera driver doesn't waste time on auto-algorithms

#### Option 3: Use Lower FPS

```json
{
  "camera": {
    "fps": 20
  }
}
```

**Expected improvement:** More headroom, more consistent timing
**Why:** 20 FPS is fine for ambient LED lighting

### If Network I/O is the Bottleneck

#### Option 1: Use Linear Format

```json
{
  "hyperhdr": {
    "use_linear_format": true
  }
}
```

**Expected improvement:** 10-15% faster
**Why:** Less data to transmit

#### Option 2: Skip Frames

In `LEDController.cpp`, modify the run loop:

```cpp
while (running_) {
    if (!processSingleFrame(false)) {
        break;
    }
    frame_count++;
    
    // Send to HyperHDR every 2nd frame only
    if (frame_count % 2 == 0 && hyperhdr_client_) {
        // Send colors
    }
}
```

**Expected improvement:** 20-30% faster
**Why:** Half the network overhead

#### Option 3: Batch Updates (Advanced)

Collect multiple frames and send them together. This reduces TCP overhead.

### If Color Extraction is the Bottleneck (less common)

#### Option 1: Reduce Regions

```json
{
  "color_extraction": {
    "horizontal_slices": 8,  // down from 10
    "vertical_slices": 6      // down from 8
  }
}
```

**Expected improvement:** 20-30% faster extraction
**Why:** Fewer regions to process

#### Option 2: Reduce Polygon Samples

```json
{
  "bezier": {
    "polygon_samples": 8  // down from 10+
  }
}
```

**Expected improvement:** 5-10% faster extraction
**Why:** Simpler polygons, less mask generation

#### Option 3: Expand NEON Usage

The current NEON optimization only covers pixel accumulation. You can also optimize:

1. **Mask Creation** (if dynamic)
2. **Color Space Conversion**
3. **Frame Scaling**

See `BOTTLENECK_ANALYSIS.md` for implementation details.

## Step 3: Understand Your Region Sizes

NEON processes 16 pixels at a time. If your regions are small, NEON overhead becomes significant.

### Calculate Your Region Sizes

For a 1920x1080 frame with edge_slices mode:
- Coverage: 20% horizontal, 15% vertical
- Horizontal slices: 10
- Vertical slices: 8

**Top/Bottom regions:**
- Width: 1920 / 10 = 192 pixels
- Height: 1080 * 0.20 = 216 pixels
- Total: **41,472 pixels per region**

**Left/Right regions:**
- Width: 1920 * 0.15 = 288 pixels
- Height: 1080 / 8 = 135 pixels
- Total: **38,880 pixels per region**

### Is NEON Worth It for Your Region Sizes?

| Region Size | NEON Benefit |
|-------------|--------------|
| < 500 pixels | ❌ Overhead dominates |
| 500-2000 pixels | ⚠️ Marginal (1.5-2x) |
| 2000-10000 pixels | ✅ Good (2-3x) |
| > 10000 pixels | ✅ Great (3-4x) |

Your regions (~40k pixels) are large enough for NEON to help, **but** if color extraction is only 10% of total time, the overall impact is still small.

## Step 4: Recommended Configuration for Raspberry Pi 5

### Optimized config.json

```json
{
  "mode": "live",
  "input_image": "",
  
  "camera": {
    "device": "/dev/video0",
    "width": 1920,
    "height": 1080,
    "fps": 25,
    "sensor_mode": 0,
    "autofocus_mode": "manual",
    "lens_position": 0.0,
    "awb_mode": "incandescent",
    "awb_gain_red": 1.5,
    "awb_gain_blue": 1.5,
    "enable_scaling": true,
    "scaled_width": 960,
    "scaled_height": 540
  },
  
  "bezier": {
    "bezier_samples": 50,
    "polygon_samples": 8
  },
  
  "color_extraction": {
    "mode": "edge_slices",
    "horizontal_coverage_percent": 20.0,
    "vertical_coverage_percent": 15.0,
    "horizontal_slices": 8,
    "vertical_slices": 6
  },
  
  "performance": {
    "enable_parallel_processing": true,
    "target_fps": 25
  },
  
  "hyperhdr": {
    "enabled": true,
    "host": "127.0.0.1",
    "port": 19400,
    "priority": 100,
    "use_linear_format": true
  }
}
```

### Expected Performance

With this configuration:
- **Before optimization:** 40-65 ms/frame (15-25 FPS)
- **After optimization:** 15-30 ms/frame (33-66 FPS)
- **Target achieved:** 25 FPS (smooth ambient lighting)

### Key Changes:
1. ✅ Camera scaling enabled (960x540)
2. ✅ Manual focus/AWB (no auto-algorithms)
3. ✅ Reduced slices (8x6 instead of 10x8)
4. ✅ Reduced polygon samples (8 instead of 10+)
5. ✅ Linear HyperHDR format
6. ✅ Target FPS set to 25

## Step 5: Monitor Performance

### Check Logs

```bash
./build/bin/app --live --verbose 2>&1 | tee performance.log

# Extract key metrics
grep "Frame I/O:" performance.log
grep "Color Extraction:" performance.log
grep "Network I/O:" performance.log
grep "Frame processed in" performance.log
```

### Key Metrics to Watch

1. **Frame I/O < 15ms:** Good
2. **Color Extraction < 5ms:** Good
3. **Network I/O < 10ms:** Good
4. **Total < 35ms (>28 FPS):** Target achieved

## Additional Optimizations (Advanced)

### 1. Multi-threaded Frame Processing

Process color extraction in parallel with I/O:

```cpp
// Thread 1: Capture next frame
// Thread 2: Process current frame
// Thread 3: Send to HyperHDR
```

**Expected improvement:** 30-50% faster (pipeline parallelism)

### 2. GPU Acceleration

Use OpenCV's CUDA/OpenCL backend for:
- Frame scaling
- Color space conversion
- Mask generation

**Expected improvement:** 40-60% faster (offload to GPU)

### 3. Custom V4L2 Capture

Bypass OpenCV's VideoCapture and use V4L2 directly with:
- Memory mapping (zero-copy)
- Buffer queuing
- Format conversion in hardware

**Expected improvement:** 20-40% faster I/O

### 4. Optimize HyperHDR Protocol

Instead of TCP sockets:
- Use UDP for lower latency
- Use Unix domain sockets for local communication
- Batch multiple updates

**Expected improvement:** 10-30% faster network

## Why NEON Alone Isn't Enough: A Visual Breakdown

```
Total Processing Time (50ms)
┌─────────────────────────────────────────────────┐
│                                                 │
│  Camera I/O (30ms) ████████████████████         │  60% - Not optimized by NEON
│                                                 │
│  Color Extraction (8ms)                         │
│    ├─ Memory Access (3ms) ████                  │  6% - Not optimized by NEON
│    ├─ NEON Loop (3ms) ████ ⚡ OPTIMIZED         │  6% - OPTIMIZED (4x speedup)
│    └─ Other (2ms) ██                            │  4% - Not optimized by NEON
│                                                 │
│  Network I/O (10ms) ██████                      │  20% - Not optimized by NEON
│                                                 │
│  Other (2ms) ██                                 │  4% - Not optimized by NEON
│                                                 │
└─────────────────────────────────────────────────┘

Even with 4x NEON speedup on 6% of total time:
  New time = 30 + (3*0.25 + 3 + 2) + 10 + 2 = 47.75ms
  Improvement = 50 - 47.75 = 2.25ms (4.5% faster)
```

## Conclusion

1. **NEON is working correctly** ✅
2. **But it only optimizes ~5-8% of total time** ⚠️
3. **Optimize I/O first** (camera, network) 🎯
4. **Then reduce data** (resolution, regions, samples)
5. **NEON will help more** once other bottlenecks are fixed

## Next Steps

1. Run `./profile_tool.sh` to measure your actual bottleneck
2. Apply optimizations in order of impact:
   - Camera scaling (biggest impact)
   - Linear HyperHDR format
   - Reduce regions/samples
   - Consider frame skipping
3. Re-measure and iterate
4. Once I/O is optimized, NEON's impact will be more visible

## Resources

- `BOTTLENECK_ANALYSIS.md` - Detailed analysis of Amdahl's Law
- `NEON_OPTIMIZATION.md` - NEON implementation details
- `profile_tool.sh` - Automated profiling tool
- `add_profiling.patch` - Add detailed timing to your code

## Questions?

Common questions:

**Q: Is NEON broken?**
A: No, it's working correctly. It's just optimizing a small part of the total time.

**Q: Should I disable NEON?**
A: No, keep it. It helps, and will help more as you optimize other parts.

**Q: What's the single biggest improvement I can make?**
A: Enable camera scaling to 960x540. This reduces I/O and processing by ~50%.

**Q: Can I reach 60 FPS on Raspberry Pi 5?**
A: Possible with aggressive optimization (small resolution, few regions, frame skipping), but 25-30 FPS is more realistic for quality ambient lighting.

**Q: How much faster is NEON really?**
A: 2-4x faster on the pixel accumulation loop itself. But that loop is only 5-8% of total time, so overall impact is 4-8%.

