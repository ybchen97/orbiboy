typedef unsigned char BYTE;
typedef char SIGNED_BYTE;
typedef unsigned short WORD; 
typedef signed short SIGNED_WORD;

class MMU {

    public:
        // Attributes
        
        // Functions
        void writeMem(WORD, BYTE);
        BYTE readMem(WORD) const;

    private:
        // Attributes
        BYTE internalMem[0x10000]; // internal memory from 0x0000 - 0xFFFF
        BYTE catridgeMem[0x200000]; // Catridge memory up to 2MB (2 * 2^20)

};