#include <bitset>
#include <cstdio>
#include <iostream>
#include <map>

extern "C" {
#include "../some_cipher.h"
}

int main(int argc, char *argv[]) {
    // 1. Read in one plaintext-ciphertext pair.
    uint16_t pt[3], ct[3];
    uint64_t pt_hex, ct_hex;
    std::cin >> std::hex >> pt_hex >> ct_hex;

    pt[0] = pt_hex >> 32;
    pt[1] = pt_hex >> 16 & 0xFFFF;
    pt[2] = pt_hex & 0xFFFF;

    ct[0] = ct_hex >> 32;
    ct[1] = ct_hex >> 16 & 0xFFFF;
    ct[2] = ct_hex & 0xFFFF;

    // 2. Create a map of partial K6s to a bitset of omega5s.
    // Read in all remaining pairs of (k6, o5) for distinguisher (44, 450) and (44, 540).
    int id;
    std::map<uint16_t, std::bitset<256>> pks0, pks1;
    uint16_t pk6, po5;
    while (std::cin >> std::dec >> id) {
        std::cin >> std::hex >> pk6 >> po5;
        if (id == 450) {
            if (pks0.find(pk6) == pks0.end()) {
                std::bitset<256> bs;
                pks0[pk6] = bs;
            }
            pks0[pk6].set(po5);
        } else if (id == 540) {
            if (pks1.find(pk6) == pks1.end()) {
                std::bitset<256> bs;
                pks1[pk6] = bs;
            }
            pks1[pk6].set(po5);
        }
    }

    // 3. After filtering for partial subkeys of round 6,
    // construct subkeys for round 6 and derive the master key.
    // Brute force on a plaintext-ciphertext pair.
    for (auto it0 = pks0.begin(); it0 != pks0.end(); ++it0) {
        uint16_t pk60 = it0->first;

        for (auto it1 = pks1.begin(); it1 != pks1.end(); ++it1) {
            uint16_t pk61 = it1->first;

            for (uint64_t i = 0; i < 65536; ++i) {
                uint16_t ks[ROUNDS+1][3] = {};

                ks[6][0] = (i & 0xFF00) | (pk61 >> 8 & 0x00F0) | (pk60 >> 12);
                ks[6][1] = (pk60 << 4 & 0xFF00) | (i & 0x00F0) | (pk61 >> 8 & 0x000F);
                ks[6][2] = (pk61 << 8 & 0xFF00) | (pk60 << 4 & 0x00F0) | (i & 0x000F);

                // 4. Derive k5 from k6 and check that the key nibbles of omega5 do not match up with an eliminated pair.

                ks[5][1] = ks[6][0] ^ ks[6][1]; // obtain the middle column to check if eliminated by (44, 450)
                uint16_t po50 = mc_inv(ks[5][1]) >> 8;
                if (it0->second[po50] == 0) continue;

                ks[5][2] = ks[6][1] ^ ks[6][2]; // obtain the last column to check if eliminated by (44, 540)
                uint16_t po51 = mc_inv(ks[5][2]) >> 8;
                if (it1->second[po51] == 0) continue;
                
                // 5. Derive the rest of k5..
                uint16_t last_col = (ks[5][2] << 4) ^ (ks[5][2] >> 12);
                ks[5][0] = (
                    (TE4[last_col >> 12] ^ RCONS[5]) << 12
                    ^ TE4[last_col >> 8 & 0xF] << 8
                    ^ TE4[last_col >> 4 & 0xF] << 4
                    ^ TE4[last_col & 0xF]
                ) ^ ks[6][0];
                // ...and up to the k0.
                for (int j = 5; j >= 1; --j) prev_key(ks[j], ks[j-1], j);
                
                // 6. Try to encrypt a plaintext with the generated keys and see if it matches.
                uint16_t output[3];
                encrypt_with_keys(pt, output, ks);

                if (output[0] == ct[0] &&
                    output[1] == ct[1] &&
                    output[2] == ct[2]) {
                    std::printf("%04x%04x%04x\n", ks[0][0], ks[0][1], ks[0][2]);
                    return 0;
                }
            }
        }
    }

    return 1;
}

