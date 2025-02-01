#ifndef PACKABLE_H
#define PACKABLE_H

class Packable {
public:
    virtual ~Packable() {}
    virtual int getSize() = 0;
    virtual int pack(uint8_t* data) = 0;
    virtual bool unpack(uint8_t* data) = 0;
};

#endif
