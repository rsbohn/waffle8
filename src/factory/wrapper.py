from ctypes import CDLL, c_void_p, c_int, c_uint16, c_uint64, CFUNCTYPE

class PDP8Emulator:
    """Python wrapper for C PDP-8 emulator core"""
    
    def __init__(self, memory_size: int = 4096):
        self.lib = CDLL('./libpdp8.so')
        self._setup_api()
        self.pdp8 = self.lib.pdp8_api_create(memory_size)
        self.devices = {}
        
    def step(self) -> int:
        """Execute one instruction, return cycles taken"""
        return self.lib.pdp8_api_step(self.pdp8)
    
    def run(self, max_cycles: int = 1000000):
        """Run for up to max_cycles"""
        # Implementation runs step() in a loop with interrupt checks
        
    def register_device(self, device: int, handler: Callable):
        """Register Python IOT handler for device code"""
        # Wrap Python callable for C callback
        
    @property
    def ac(self) -> int:
        return self.lib.pdp8_api_get_ac(self.pdp8)
    
    @ac.setter
    def ac(self, value: int):
        self.lib.pdp8_api_set_ac(self.pdp8, value & 0xFFF)