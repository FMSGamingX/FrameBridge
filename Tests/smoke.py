# FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
"""Exercise the shipped DLL, Windows decoder and presentation path through a real HWND."""
import argparse
import ctypes as ct
from ctypes import wintypes as W
from pathlib import Path
import sys
import os
import time
from make_sample import make

# Windows pipes may default to a legacy code page even for Unicode file paths.
if not sys.stdout.isatty():
    sys.stdout.reconfigure(encoding="utf-8")

root = Path(__file__).resolve().parents[1]
dll = ct.CDLL(str(root/"bin"/"FrameBridge.Engine.dll"))
user = ct.windll.user32

class Config(ct.Structure):
    _fields_ = [(k, ct.c_int) for k in "kind width height profile backend quality reduceArtifacts generate refresh buffer desktopX desktopY desktopW desktopH vsync exclusive outputW outputH rateNum rateDen inputMilliFps hudAware".split()]

class Stats(ct.Structure):
    _fields_ = ([(k, ct.c_int) for k in "state width height activeGeneration".split()]
               + [(k, ct.c_int64) for k in "inputs outputs generated dropped processed jobs".split()]
               + [(k, ct.c_double) for k in "workMs generationMs latencyMs rate gpuTotalMs".split()]+[("gpuJobs",ct.c_int64),("pacingErrorMs",ct.c_double),("intervals",ct.c_int64),("hdr",ct.c_int)])

dll.fb_start.argtypes = [W.HWND, ct.POINTER(Config), W.LPCWSTR]
dll.fb_start.restype = ct.c_void_p
dll.fb_start_ai.argtypes = dll.fb_start.argtypes+[W.LPCWSTR]
dll.fb_start_ai.restype = ct.c_void_p
dll.fb_start_nvidia_flow.argtypes = dll.fb_start.argtypes
dll.fb_start_nvidia_flow.restype = ct.c_void_p
dll.fb_stats.argtypes = [ct.c_void_p, ct.POINTER(Stats), W.LPWSTR, ct.c_int, W.LPWSTR, ct.c_int]
dll.fb_close.argtypes = [ct.c_void_p]
dll.fb_device.argtypes = [ct.c_int, W.LPWSTR, ct.c_int, W.LPWSTR, ct.c_int]
user.CreateWindowExW.restype = W.HWND
user.CreateWindowExW.argtypes = [W.DWORD,W.LPCWSTR,W.LPCWSTR,W.DWORD,ct.c_int,ct.c_int,ct.c_int,ct.c_int,W.HWND,W.HMENU,W.HINSTANCE,W.LPVOID]
user.DestroyWindow.argtypes = [W.HWND]

def run(hwnd, config, key="", seconds=3, expected_error=False,model=None, nvidia=False):
    s = (dll.fb_start_nvidia_flow(hwnd, ct.byref(config), key) if nvidia
         else dll.fb_start_ai(hwnd, ct.byref(config), key, model))
    assert s, "session creation failed"
    info, backend, msg = Stats(), ct.create_unicode_buffer(256), ct.create_unicode_buffer(1024)
    started = time.monotonic()
    try:
        active = None
        while time.monotonic()-started < seconds+30 and (active is None or time.monotonic()-active < seconds):
            event = W.MSG()
            while user.PeekMessageW(ct.byref(event), None, 0, 0, 1):
                user.TranslateMessage(ct.byref(event))
                user.DispatchMessageW(ct.byref(event))
            dll.fb_stats(s, ct.byref(info), backend, len(backend), msg, len(msg))
            if info.outputs and active is None: active=time.monotonic()
            if info.state in (3,4): break
            time.sleep(.006)
        elapsed = time.monotonic()-started
        active_rate = max(0,info.outputs-1)/max(.001,time.monotonic()-(active or started))
        gpu_ms=f"{info.gpuTotalMs/info.gpuJobs:.2f} ms" if info.gpuJobs else "N/A"
        pacing=f"{info.pacingErrorMs/info.intervals:.2f} ms" if info.intervals else "N/A"
        print(f"{['synthetic','file','capture','desktop'][config.kind]} | {backend.value} | {info.width}x{info.height} | {info.inputs} inputs, {info.outputs} presented, {info.generated} generated, {info.dropped} dropped | {active_rate:.1f} active presents/s ({elapsed:.2f}s including startup) | work {info.workMs/max(1,info.processed):.2f} ms | GPU {gpu_ms} ({info.gpuJobs} samples) | pacing MAE {pacing} | {msg.value}", flush=True)
        if expected_error:
            assert info.state == 4 and msg.value
        else:
            assert info.state != 4 and info.inputs >= 2 and info.outputs >= 2, msg.value
            if nvidia:
                assert "NVIDIA Optical Flow" in backend.value and info.generated > 0
        return info
    finally:
        dll.fb_close(s)

def capture_test(hwnd, config, nvidia=False):
    name,key=ct.create_unicode_buffer(512),ct.create_unicode_buffer(4096)
    assert dll.fb_device(0,name,len(name),key,len(key))==1
    print("Opening",name.value,flush=True)
    config.kind=2
    profiles=ct.create_unicode_buffer(65000)
    dll.fb_profiles.argtypes=[W.LPCWSTR,W.LPWSTR,ct.c_int]
    assert dll.fb_profiles(key.value,profiles,len(profiles))==1
    for row in profiles.value.splitlines():
        if "1920 × 1080" in row and "30.00 fps" in row:
            config.profile=int(row.split("|")[0])
            print("Capture profile:",row,flush=True)
            break
    run(hwnd,config,key.value,8,nvidia=nvidia)

if __name__=="__main__":
    p=argparse.ArgumentParser()
    p.add_argument("--ai", action="store_true")
    p.add_argument("--output-only", action="store_true",help="Play the file fixture with generation disabled for external presentation telemetry.")
    p.add_argument("--nvidia-flow", action="store_true")
    p.add_argument("--device", action="store_true")
    p.add_argument("--interpolate", action="store_true")
    p.add_argument("--backend", type=int, default=-2)
    p.add_argument("--seconds", type=float, default=3)
    args=p.parse_args()
    ct.windll.ole32.CoInitializeEx(None,0)
    user.SetProcessDPIAware()
    name,rect=ct.create_unicode_buffer(64),W.RECT()
    assert dll.fb_test_display(name,len(name),ct.byref(rect)), "Secondary 75 Hz monitor unavailable"
    hz=dll.fb_refresh(name.value)
    print(f"Test display {name.value}, desktop {hz} Hz",flush=True)
    hwnd=user.CreateWindowExW(0,"STATIC","FrameBridge pipeline test",0x90000000,rect.left+24,rect.top+32,512,288,None,None,None,None)
    assert hwnd
    c=Config(0,1920,1080,-1,args.backend,1,1,int(args.interpolate),hz,1,rect.left,rect.top,320,180)
    c.hudAware=1
    try:
        if args.output_only:
            sample=root/"Tests"/"results"/"motion-30.avi"
            if not sample.exists(): make(sample)
            c.kind,c.generate=1,0
            info=run(hwnd,c,str(sample),7)
            assert info.state==3 and info.inputs==120 and info.generated==0
            sys.exit(0)
        if args.nvidia_flow:
            c.generate=1
            run(hwnd,c,seconds=args.seconds,nvidia=True)
            sample=root/"Tests"/"results"/"motion-30.avi"
            if not sample.exists(): make(sample)
            c.kind=1
            run(hwnd,c,str(sample),7,nvidia=True)
            c.kind=3
            run(hwnd,c,nvidia=True)
            if args.device:
                capture_test(hwnd,c,nvidia=True)
            c.kind=0
            c.backend=-2
            run(hwnd,c,expected_error=True,nvidia=True)
            sys.exit(0)
        if args.ai:
            model=str(Path(os.environ["LOCALAPPDATA"])/"FrameBridge/Models/RIFE-4.9/rife49.onnx")
            c.width,c.height,c.quality,c.buffer,c.generate=320,180,0,2,1
            info=run(hwnd,c,seconds=args.seconds,model=model)
            assert info.generated>0, "Inference never presented a midpoint"
            run(hwnd,c,model=str(root/"missing.onnx"),expected_error=True)
            sys.exit(0)
        synthetic=run(hwnd,c,seconds=args.seconds)
        if args.interpolate: assert synthetic.generated>0
        c.kind=3
        run(hwnd,c)
        sample=root/"Tests"/"results"/"motion-30.avi"
        if not sample.exists(): make(sample)
        c.kind=1
        file=run(hwnd,c,str(sample),7)
        assert file.inputs==120 and file.state==3
        run(hwnd,c,str(root/"missing-video.avi"),2,True)
        if args.device:
            capture_test(hwnd,c)
    finally:
        user.DestroyWindow(hwnd)
        ct.windll.ole32.CoUninitialize()

