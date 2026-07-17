import sys
import atexit
from types import TracebackType
from typing import Optional, Type

if sys.platform == 'win32':
    import msvcrt
else:
    import termios
    import tty
    import select


class TermInput:
    """
    Manages high-performance cross-platform unbuffered terminal input.
    Sets raw mode once at initialization and restores settings on exit.
    """

    def __init__(self) -> None:
        self._is_windows: bool = (sys.platform == 'win32')
        self._old_settings: Optional[list] = None
        self._active: bool = False


    def start(self) -> None:
        """Enables unbuffered raw input mode and registers exit handlers."""
        if self._active:
            return
        
        if not self._is_windows:
            # Capture original terminal settings
            self._old_settings = termios.tcgetattr(sys.stdin)
            # Switch to cbreak mode (uncooked, no line buffering, echoes disabled)
            tty.setcbreak(sys.stdin.fileno())
            
        self._active = True
        # Ensure terminal is ALWAYS restored, even if the script crashes or exit() is called
        atexit.register(self.stop)


    def stop(self) -> None:
        """Restores the terminal to its original cooked configuration."""
        if not self._active:
            return
            
        if not self._is_windows and self._old_settings is not None:
            # Restore original terminal state smoothly after flushing queues
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, self._old_settings)
            
        self._active = False
        # Clean up the atexit registry to prevent memory leaks
        try:
            atexit.unregister(self.stop)
        except ValueError:
            pass


    def getch(self) -> Optional[str]:
        """
        Polls for a waiting character. 
        Zero system-call overhead on mode toggling. Very CPU efficient.
        """
        if not self._active:
            raise RuntimeError("TermInput must be started before polling.")

        if self._is_windows:
            if msvcrt.kbhit():
                return msvcrt.getch().decode('utf-8', errors='ignore')
            return None
        else:
            rlist, _, _ = select.select([sys.stdin], [], [], 0)
            if rlist:
                return sys.stdin.read(1)
            return None



# --- Example Usage ---
if __name__ == "__main__":
    import time

    print("Initializing terminal session...")
    
    # Using 'with' is recommended as it enforces localized terminal scoping
    io = TermInput()
    io.start()
    print("Loop started. Type anything! Press 'q' to quit.")
        
    while True:
        char = io.getch()
        if char:
            # \r resets cursor to start of line to bypass missing newlines in raw mode
            print(f"\rCaptured key: {repr(char)}    ", end="", flush=True)
            if char.lower() == 'q':
                break
                    
        # Tiny sleep prevents 100% CPU thread starvation
        time.sleep(0.01)

    print("\nTerminal successfully restored to normal mode.")
