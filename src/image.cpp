#include <cassert>
#include "image.h"
#include "qtable.h"
#include "htables.h"
#include "markers.h"


void YAJLImage::set_src(std::string filename) {
    bstream = new bitio::bitio_stream(filename, bitio::READ);
}

void YAJLImage::scan_markers() {
    // First check for SOI marker
    if (bstream->read(0x10) != markers::SOI) {
        std::cout << "SOI not found" << std::endl;
        return;
    }
    bool continue_scan = true;
    while (continue_scan) {
        uint16_t marker = bstream->read(0x10);
        assert(markers::is_marker(marker));

        switch (marker) {
            case markers::APP0: {
                if (!use_minimal_scan) {
                    // param-length followed by application data.
                    uint16_t length = bstream->read(0x10);
                    // exlude Lp from length
                    length -= 2;
                    auto *app_data = new char[length];
                    for (int i = 0; i < length; i++) {
                        app_data[i] = bstream->read(0x8);
                    }
                    push_app_data(app_data);
                } else {
                    uint16_t length = bstream->read(0x10);
                    length -= 2;
                    for (int i = 0; i < length; i++) {
                        bstream->read(0x8);
                    }
                }
                break;
            }
            case markers::COM: {
                if (!use_minimal_scan) {
                    // param-length followed by comment-bytes.
                    uint16_t length = bstream->read(0x10);
                    length -= 2;
                    auto *com_data = new char[length];
                    for (int i = 0; i < length; i++) {
                        com_data[i] = bstream->read(0x8);
                    }
                    push_com_data(com_data);
                } else {
                    uint16_t length = bstream->read(0x10);
                    length -= 2;
                    for (int i = 0; i < length; i++) {
                        bstream->read(0x8);
                    }
                }
                break;
            }
            case markers::DQT: {
                // first we read 16-bit Lq
                uint32_t seg_len = bstream->read(0x10);
                // exclude size of Lq which is 2 bytes
                seg_len -= 2;
                while (seg_len > 0) {
                    YAJLQTable table{};
                    table.precision = bstream->read(0x4);
                    table.dest_id = bstream->read(0x4);
                    uint8_t qk_size = table.precision == 0 ? 8 : 16;
                    for (int i : scanorders::ZZ) {
                        table.coeffs[i] = bstream->read(qk_size);
                    }
                    add_qtable(table, table.dest_id);
                    seg_len -= 1 + (qk_size << 3);
                }
                break;
            }
            default: {
#ifdef DEBUG
                printf("Unhandled marker: %X\n", marker);
#endif
                continue_scan = false;
            }
        }
    }

}

YAJLImage::YAJLImage(bool _use_minimal_scan) {
    bstream = nullptr;
    use_minimal_scan = _use_minimal_scan;
}


char *YAJLImageDescriptor::get_app_data(uint32_t index) {
    if (index < ds_app_data.size()) {
        return ds_app_data[index];
    } else {
        return nullptr;
    }
}

void YAJLImageDescriptor::push_app_data(char *app_data) {
    ds_app_data.push_back(app_data);
}

void YAJLImageDescriptor::push_com_data(char *com_data) {
    ds_comments.push_back(com_data);
}

char *YAJLImageDescriptor::get_comment(uint32_t index) {
    if (index < ds_comments.size()) {
        return ds_comments[index];
    } else {
        return nullptr;
    }
}

void YAJLImageDescriptor::add_qtable(YAJLQTable table, int index) {
    assert(index < 4);
    tables[index] = table;
}

YAJLQTable YAJLImageDescriptor::get_qtable(uint8_t index) {
    assert(index < 4);
    return tables[index];
}
