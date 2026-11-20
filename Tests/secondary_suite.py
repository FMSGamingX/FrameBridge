# FrameBridge • Faris Alsari / FMSGaming • Famous Gaming
"""Run the built application's interactive checks on the secondary 75 Hz display."""
import ctypes as ct
from ctypes import wintypes as W
from pathlib import Path
import subprocess
import sys
import time
from smoke import dll, user, root


class Mode(ct.Structure):
    # Win32 DEVMODEW display variant, including the printer/display union.
    _fields_ = [("device", W.WCHAR*32), ("spec", W.WORD), ("driver", W.WORD),
                ("size", W.WORD), ("extra", W.WORD), ("fields", W.DWORD),
                ("position", W.POINT), ("orientation", W.DWORD), ("fixedOutput", W.DWORD),
                ("printer", W.SHORT*5), ("form", W.WCHAR*32), ("logPixels", W.WORD),
                ("bits", W.DWORD), ("width", W.DWORD), ("height", W.DWORD),
                ("flags", W.DWORD), ("hz", W.DWORD), ("tail", W.DWORD*8)]


def modes(name, index):
    m=Mode()
    m.size=ct.sizeof(m)
    return m if user.EnumDisplaySettingsW(name,index,ct.byref(m)) else None


def change(name, mode):
    result=user.ChangeDisplaySettingsExW(name,ct.byref(mode),None,4,None) # CDS_FULLSCREEN, session only
    if result:
        raise RuntimeError(f"Windows display switch failed ({result})")


def focus_output(process, bounds):
    # Exclusive DXGI requires a foreground window. Click only the test's own
    # unobscured output, inside the approved monitor, then return the pointer.
    user.FindWindowW.restype=W.HWND
    user.WindowFromPoint.restype=W.HWND
    user.WindowFromPoint.argtypes=[W.POINT]
    handle=user.FindWindowW(None,"FrameBridge · Fullscreen output")
    if not handle:
        return False
    pid=W.DWORD()
    user.GetWindowThreadProcessId(W.HWND(handle),ct.byref(pid))
    r=W.RECT()
    if pid.value!=process.pid or not user.GetWindowRect(W.HWND(handle),ct.byref(r)):
        return False
    p=W.POINT(r.left+40,r.top+40)
    if not(bounds.left<=p.x<bounds.right and bounds.top<=p.y<bounds.bottom) or user.WindowFromPoint(p)!=handle:
        return False
    old=W.POINT()
    if not user.GetCursorPos(ct.byref(old)) or not user.SetCursorPos(p.x,p.y):
        return False
    try:
        if user.WindowFromPoint(p)!=handle:
            return False
        user.mouse_event(2,0,0,0,0)
        user.mouse_event(4,0,0,0,0)
        return True
    finally:
        user.SetCursorPos(old.x,old.y)


def run():
    user.SetProcessDPIAware()
    name,rect=ct.create_unicode_buffer(64),W.RECT()
    if not dll.fb_test_display(name,len(name),ct.byref(rect)):
        raise RuntimeError("Secondary 75 Hz monitor unavailable")
    original=modes(name.value,-1)
    idx=0
    while True:
        selected=modes(name.value,idx)
        if selected is None:
            raise RuntimeError("75 Hz mode unavailable")
        if (selected.width,selected.height,selected.bits)==(original.width,original.height,original.bits) and selected.hz in (74,75):
            break
        idx+=1
    try:
        change(name.value,selected)
        current=modes(name.value,-1)
        if current.hz not in (74,75):
            raise RuntimeError(f"Requested 75 Hz, measured display configuration {current.hz}")
        print(f"{name.value} test mode: {current.width}x{current.height} at {current.hz} Hz",flush=True)
        cache=root/"bin"/".build"
        for test in [cache/"FlowTest.exe",cache/"GpuFlowTest.exe"]:
            subprocess.run([str(test)],check=True)
        app=str(root/"bin"/"FrameBridge.exe")
        subprocess.run([app,"--shell-test"],check=True)
        for index in range(16):
            if dll.fb_nvidia_flow_adapter(index) and '--ui-only' not in sys.argv:
                subprocess.run([sys.executable,str(root/"Tests"/"smoke.py"),"--nvidia-flow",
                                "--backend",str(index),"--seconds","10","--device"],check=True)
        for options in [["--interpolate","--backend","0","--device","--seconds","10"],
                        ["--interpolate","--backend","1"],["--interpolate","--backend","-2"],
                        ["--ai","--backend","0"],["--ai","--backend","-2"]]:
            if '--ui-only' not in sys.argv:
                subprocess.run([sys.executable,str(root/"Tests"/"smoke.py"),*options],check=True)
        result=root/"Tests"/"results"
        with subprocess.Popen([app,"--ui-test",str(result)]) as process:
            while process.poll() is None and not focus_output(process,rect):
                time.sleep(.025)
            if process.wait(timeout=45):
                raise RuntimeError((result/"ui-error.txt").read_text(encoding="utf-8-sig"))
        print((result/"ui-test.txt").read_text(encoding="utf-8-sig"),flush=True)
    finally:
        change(name.value,original)
        restored=modes(name.value,-1)
        if (restored.width,restored.height,restored.hz)!=(original.width,original.height,original.hz):
            raise RuntimeError("Could not verify desktop restoration")
        print(f"{name.value} restored to {restored.width}x{restored.height} at {restored.hz} Hz",flush=True)


if __name__=="__main__":
    run()

