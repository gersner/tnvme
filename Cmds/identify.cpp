#include "identify.h"
#include "../Utils/buffers.h"


#define CNS_BITMASK         0x01

const uint16_t Identify::IDEAL_DATA_SIZE =  4096;

// Register metrics (ID Cmd Ctrlr Cap struct) to aid interfacing with the dnvme
#define ZZ(a, b, c, d)         { b, c, d },
IdentifyDataType Identify::mIdCtrlrCapMetrics[] =
{
    IDCTRLRCAP_TABLE
};
#undef ZZ

// Register metrics (ID Cmd namespace struct) to aid interfacing with the dnvme
#define ZZ(a, b, c, d)         { b, c, d },
IdentifyDataType Identify::mIdNamespcType[] =
{
    IDNAMESPC_TABLE
};
#undef ZZ


Identify::Identify() :
    Cmd(0, Trackable::OBJTYPE_FENCE)
{
    // This constructor will throw
}


Identify::Identify(int fd) :
    Cmd(fd, Trackable::OBJ_IDENTIFY)
{
    Init(CMD_ADMIN, 0x06, DATADIR_FROM_DEVICE, 64);
    SetCNS(true);
}


Identify::~Identify()
{
}


void
Identify::SetCNS(bool ctrlr)
{
    uint8_t curVal = GetByte(10, 0);
    if (ctrlr)
        curVal |= CNS_BITMASK;
    else
        curVal &= ~CNS_BITMASK;
    SetByte(curVal, 10, 0);
}


bool
Identify::GetCNS()
{
    uint8_t curVal = GetByte(10, 0);
    if (curVal & CNS_BITMASK)
        return true;
    return false;
}


uint64_t
Identify::GetValue(IdCtrlrCap field)
{
    if (field >= IDCTRLRCAP_FENCE) {
        LOG_DBG("Unknown ctrlr cap field: %d", field);
        throw exception();
    }

    return GetValue(field, mIdCtrlrCapMetrics);
}


uint64_t
Identify::GetValue(IdNamespc field)
{
    if (field >= IDNAMESPC_FENCE) {
        LOG_DBG("Unknown namespace field: %d", field);
        throw exception();
    }
    return GetValue(field, mIdNamespcType);
}


uint64_t
Identify::GetValue(int field, IdentifyDataType *idData)
{
    uint8_t byte;
    uint64_t value = 0;

    if (idData[field].length >= sizeof(uint64_t)) {
        LOG_DBG("sizeof(%s) > %ld bytes", idData[field].desc, sizeof(uint64_t));
        throw exception();
    } else if ((idData[field].length + idData[field].offset) >=
        GetROPrpBufferSize()) {
        LOG_DBG("Detected illegal definition in IDxxxxx_TABLE");
        LOG_DBG("Reference calc (%d): %d + %d >= %ld", field,
            idData[field].length, idData[field].offset, GetROPrpBufferSize());
        throw exception();
    }

    for (int i = 0; i < idData[field].length; i++) {
        byte = (GetROPrpBuffer())[idData[field].offset + i];
        value |= ((uint64_t)byte << i);
    }
    LOG_NRM("%s = 0x%08lX", idData[field].desc, value);
    return value;
}


void
Identify::Dump(LogFilename filename, string fileHdr)
{
    FILE *fp;
    const uint8_t *buf = GetROPrpBuffer();
    string objName = "Admin Cmd: Identify";


    // Do a raw dump of the data
    Buffers::Dump(filename, buf, 0, ULONG_MAX, GetROPrpBufferSize(), fileHdr);

    // Reopen the file and append the same data in a different format
    if ((fp = fopen(filename.c_str(), "a")) == NULL) {
        LOG_DBG("Failed to open file: %s", filename.c_str());
        throw exception();
    }

    // How do we interpret the data contained herein?
    if (GetCNS()) {
        for (int i = IDCTRLRCAP_VID; i < IDCTRLRCAP_FENCE; i++)
            Dump(fp, i, mIdCtrlrCapMetrics);
    } else {
        for (int i = IDNAMESPC_NSZE; i < IDNAMESPC_FENCE; i++)
            Dump(fp, i, mIdNamespcType);
    }
    fclose(fp);
}


void
Identify::Dump(FILE *fp, int field, IdentifyDataType *idData)
{
    const uint8_t *data;
    const int BUF_SIZE = 20;
    char work[BUF_SIZE];
    string output;
    unsigned long dumpLen = idData[field].length;

    fprintf(fp, "\n%s\n", idData[field].desc);

    data = &((GetROPrpBuffer())[idData[field].offset]);
    if ((idData[field].length + idData[field].offset) > GetROPrpBufferSize()) {
        LOG_DBG("Detected illegal definition in IDxxxxx_TABLE");
        LOG_DBG("Reference calc (%d): %d + %d >= %ld", field,
            idData[field].length, idData[field].offset, GetROPrpBufferSize());
        throw exception();
    }

    unsigned long addr = idData[field].offset;
    for (unsigned long j = 0; j < dumpLen; j++, addr++) {
        if ((j % 16) == 15) {
            snprintf(work, BUF_SIZE, " %02X\n", *data++);
            output += work;
            fprintf(fp, "%s", output.c_str());
            output.clear();
        } else if ((j % 16) == 0) {
            snprintf(work, BUF_SIZE, "0x%08X: %02X",
                (uint32_t)addr, *data++);
            output += work;
        } else {
            snprintf(work, BUF_SIZE, " %02X", *data++);
            output += work;
        }
    }
    if (output.length() != 0)
        fprintf(fp, "%s\n", output.c_str());
}
