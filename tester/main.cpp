#include <fstream>
#include <cstdio>
#include <cstring>

#include <z80emu.h>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;
using namespace llz80emu;

/* I/O and memory spaces */
#define MEM_SIZE                        65536 // 64K address space
uint8_t memory[MEM_SIZE];
uint8_t io[MEM_SIZE];

/* our bus state definitions*/
#define Z80_BUS_RD                      (1 << 0)
#define Z80_BUS_WR                      (1 << 1)
#define Z80_BUS_MREQ                    (1 << 2)
#define Z80_BUS_IORQ                    (1 << 3)
static inline uint8_t get_bus_state(z80_pinbits_t pins) {
    return (
        ((pins & Z80_RD) ? 0 : Z80_BUS_RD) | // active low signals
        ((pins & Z80_WR) ? 0 : Z80_BUS_WR) |
        ((pins & Z80_MREQ) ? 0 : Z80_BUS_MREQ) |
        ((pins & Z80_IORQ) ? 0 : Z80_BUS_IORQ)
    );
}
static inline uint8_t get_bus_state(const string& pins) {
    return (
        ((pins[0] == 'r') ? Z80_BUS_RD : 0) |
        ((pins[1] == 'w') ? Z80_BUS_WR : 0) |
        ((pins[2] == 'm') ? Z80_BUS_MREQ : 0) |
        ((pins[3] == 'i') ? Z80_BUS_IORQ : 0)
    );
}

int main(int argc, const char** argv) {
    int tests_ran = 0, tests_failed = 0;

    /* go through each supplied test cases file */
    for(int arg_idx = 1; arg_idx < argc; arg_idx++) {
        ifstream tc_file(argv[arg_idx]);
        if(!tc_file) {
            printf("ERROR: cannot open test cases file %s, ignoring\n", argv[arg_idx]);
            continue;
        }
        json tc = json::parse(tc_file); // parse JSON file
        // TODO: add checks if JSON is successfully parsed

        for(auto& test : tc) {
            tests_ran++;

            string name = test["name"].template get<string>(); // test name
            z80emu z80(false); // Z80 emulator instance (we create one for each test)
            z80_pins_t pins = Z80_PINS_INIT; pins.state &= ~Z80_RESET; // pull RESET low for POR
            for(int i = 0; i < 2; i++) pins = z80.clock(pins.state); // POR (1 high + 1 low)

            /* run first cycle of NOP before injecting new register values (as we bring ourselves out of reset) */
            memory[0] = 0x00; // NOP instruction
            for(int i = 0; i < 8; i++) { // 4 cycles in total
                pins.state |= Z80_RESET | Z80_BUSREQ | Z80_WAIT | Z80_INT; // pull RESET high, end POR (plus BUSREQ and WAIT too, otherwise we'd be stuck)
                pins = z80.clock(pins.state);
            }

            /* populate registers */
            z80_registers_t regs;
            regs.REG_PC = test["initial"]["pc"];
            regs.REG_SP = test["initial"]["sp"];
            regs.REG_A = test["initial"]["a"];
            regs.REG_B = test["initial"]["b"];
            regs.REG_C = test["initial"]["c"];
            regs.REG_D = test["initial"]["d"];
            regs.REG_E = test["initial"]["e"];
            regs.REG_F = test["initial"]["f"];
            regs.REG_H = test["initial"]["h"];
            regs.REG_L = test["initial"]["l"];
            regs.REG_I = test["initial"]["i"];
            regs.REG_R = test["initial"]["r"];
            regs.REG_IX = test["initial"]["ix"];
            regs.REG_IY = test["initial"]["iy"];
            regs.REG_AF_S = test["initial"]["af_"];
            regs.REG_BC_S = test["initial"]["bc_"];
            regs.REG_DE_S = test["initial"]["de_"];
            regs.REG_HL_S = test["initial"]["hl_"];
            regs.iff1 = ((int)test["initial"]["iff1"] == 1);
            regs.iff2 = ((int)test["initial"]["iff2"] == 1);
            z80.set_regs(regs);

            /* populate initial memory space */
            for(auto& entry : test["initial"]["ram"]) {
                uint16_t addr = entry[0]; uint8_t data = entry[1];
                memory[addr] = data;
            }

            /* populate initial I/O space */
            for(auto& entry : test["ports"]) {
                if((entry[2].template get<string>())[0] == 'r') {
                    /* read entry */
                    uint16_t addr = entry[0]; uint8_t data = entry[1];
                    io[addr] = data;
                }
            }

            /* start clocking the CPU */
            int cycle_cnt = 0; // cycle count
            for(auto& cycle : test["cycles"]) {
                uint8_t states[2]; // bus state on rising and falling edges
                for(int i = 0; i < 2; i++) {
                    pins.state |= Z80_RESET | Z80_BUSREQ | Z80_WAIT | Z80_INT; // pull RESET high, end POR (plus BUSREQ and WAIT too, otherwise we'd be stuck)
                    pins = z80.clock(pins.state);
                    states[i] = get_bus_state(pins.state); // log bus state
                    // printf("0x%x\n", states[i]);

                    uint16_t addr = (pins.state & Z80_A_ALL) >> Z80_PIN_A_BASE; // get address bus
                    if(states[i] & Z80_BUS_RD) {
                        if(states[i] & Z80_BUS_MREQ)
                            pins.state = (pins.state & ~Z80_D_ALL) | ((z80_pinbits_t)memory[addr] << Z80_PIN_D_BASE);
                        else if(states[i] & Z80_BUS_IORQ)
                            pins.state = (pins.state & ~Z80_D_ALL) | ((z80_pinbits_t)io[addr] << Z80_PIN_D_BASE);
                    } else if(states[i] & Z80_BUS_WR) {
                        uint8_t data = (pins.state & Z80_D_ALL) >> Z80_PIN_D_BASE; // get data bus
                        if(states[i] & Z80_BUS_MREQ) memory[addr] = data;
                        else if(states[i] & Z80_BUS_IORQ) io[addr] = data;
                    }
                }
                uint8_t state = states[0] & states[1]; // we are only interested in states that persist across both edges
                uint8_t desired_state = get_bus_state(cycle[2].template get<string>());

                // printf("0x%x 0x%x\n", states[0], states[1]);
                if(state != desired_state) {
                    printf("%s cycle %d: bus state 0x%x (0x%x 0x%x), expected 0x%x\n", name.c_str(), cycle_cnt + 1, state, states[0], states[1], desired_state);
                    goto fail;
                }

                if(cycle[0] != nullptr) {
                    /* check address bus */
                    uint16_t addr = (pins.state & Z80_A_ALL) >> Z80_PIN_A_BASE; // get address bus
                    uint16_t desired_addr = cycle[0];
                    if(addr != desired_addr) {
                        printf("%s cycle %d: addr 0x%04x, expected 0x%04x\n", name.c_str(), cycle_cnt + 1, addr, desired_addr);
                        goto fail;
                    }   
                }

                if(cycle[1] != nullptr) {
                    /* check data bus */
                    uint8_t data = (pins.state & Z80_D_ALL) >> Z80_PIN_D_BASE; // get data bus
                    uint8_t desired_data = cycle[1];
                    if(data != desired_data) {
                        printf("%s cycle %d: data 0x%02x, expected 0x%02x\n", name.c_str(), cycle_cnt + 1, data, desired_data);
                        goto fail;
                    }  
                }

                cycle_cnt++;
            }

            /* check registers after execution */
            regs = z80.get_regs();
#define check_register(val, expected, reg_name, len) \
            { \
                int expected_val = expected; \
                if((int)val != expected_val) { \
                    printf("%s: " reg_name " 0x%0" len "x, expected 0x%0" len "x\n", name.c_str(), (int)val, expected_val); \
                    goto fail; \
                } \
            }
            check_register(regs.REG_A, test["final"]["a"], "A", "2");
            check_register(regs.REG_B, test["final"]["b"], "B", "2");
            check_register(regs.REG_C, test["final"]["c"], "C", "2");
            check_register(regs.REG_D, test["final"]["d"], "D", "2");
            check_register(regs.REG_E, test["final"]["e"], "E", "2");
            check_register(regs.REG_F, test["final"]["f"], "F", "2");
            check_register(regs.REG_H, test["final"]["h"], "H", "2");
            check_register(regs.REG_L, test["final"]["l"], "L", "2");
            check_register(regs.REG_I, test["final"]["i"], "I", "2");
            check_register(regs.REG_R, test["final"]["r"], "R", "2");
            check_register(regs.REG_AF_S, test["final"]["af_"], "AF'", "4");
            check_register(regs.REG_BC_S, test["final"]["bc_"], "BC'", "4");
            check_register(regs.REG_DE_S, test["final"]["de_"], "DE'", "4");
            check_register(regs.REG_HL_S, test["final"]["hl_"], "HL'", "4");
            check_register(regs.REG_IX, test["final"]["ix"], "IX", "4");
            check_register(regs.REG_IY, test["final"]["iy"], "IY", "4");
            check_register(regs.REG_PC, test["final"]["pc"], "PC", "4");
            check_register(regs.REG_SP, test["final"]["sp"], "SP", "4");
            check_register(regs.iff1, test["final"]["iff1"], "IFF1", "1");
            check_register(regs.iff2, test["final"]["iff2"], "IFF2", "1");

            /* check memory after execution */
            for(auto& entry : test["final"]["ram"]) {
                uint16_t addr = entry[0]; uint8_t data = entry[1];
                if(memory[addr] != data) {
                    printf("%s: memory @ 0x%04x = 0x%02x, expected 0x%02x\n", name.c_str(), addr, memory[addr], data);
                    goto fail;
                }
            }

            /* check I/O after execution */
            for(auto& entry : test["ports"]) {
                if((entry[2].template get<string>())[0] == 'w') {
                    /* write entry */
                    uint16_t addr = entry[0]; uint8_t data = entry[1];
                    if(io[addr] != data) {
                        printf("%s: I/O @ 0x%04x = 0x%02x, expected 0x%02x\n", name.c_str(), addr, io[addr], data);
                        goto fail;
                    }
                }
            }
            
            continue; // success
fail:
            tests_failed++;
        }
    }

    if(tests_ran == 0) {
        printf("ERROR: no test cases provided\n");
        return -1;
    }

    printf("testing finished, %d out of %d tests were successful\n", tests_ran - tests_failed, tests_ran);
    return tests_failed;
}