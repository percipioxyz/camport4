// FileAccessControl.cpp
#include "FileAccessControl.hpp"
#include <cstring>
#include <string>
// #include "TYFileAccessController.hpp"
#include "Utils.hpp"

namespace percipio {

FileAccessControlBuf::FileAccessControlBuf(TY_DEV_HANDLE hDevice, size_t bufferSize)
    : m_hDevice(hDevice)
    , m_fileSelector("")
    , m_buffer(bufferSize)
    , m_bufferSize(bufferSize)
    , m_currentPosition(0)
    , m_isOpen(false)
{
    //Set buffer
    char* base = &m_buffer.front();
    setg(base, base, base); // input buffer
    setp(base, base + m_bufferSize); // output buffer
}

FileAccessControlBuf::~FileAccessControlBuf() {
    if (m_isOpen) {
        close();
    }
}

bool FileAccessControlBuf::open(const char *file_name, std::ios::openmode mode) {
    m_fileSelector = file_name;
    
    TY_FILE_OPEN_MODE openMode;
    if (mode & std::ios::trunc) {
        if (!deleteFile()) {
            return false;
        }
    }
    if ((mode & std::ios::out) && (mode & std::ios::in)) {
        openMode = FILE_OPEN_MODE_READWRITE;
    } else if ((mode & std::ios::out) || (mode & std::ios::trunc)) {
        openMode = FILE_OPEN_MODE_WRITE;
    } else {
        openMode = FILE_OPEN_MODE_READ;
    }
    
    m_isOpen = openFile(openMode);
    if (m_isOpen) {
        m_currentPosition = 0;
        // reset buffer
        char* base = &m_buffer.front();
        setg(base, base, base);
        setp(base, base + m_bufferSize);
    }
    
    return m_isOpen;
}

bool FileAccessControlBuf::close() {
    if (!m_isOpen) return true;
    
    // sync buffer
    sync();

    m_isOpen = !closeFile();
    
    return !m_isOpen;
}

bool FileAccessControlBuf::openFile(TY_FILE_OPEN_MODE openMode) {
    int64_t op_status= 0;
    ASSERT_OK(TYEnumSetString(m_hDevice, "FileSelector", m_fileSelector.c_str()));
    // LOGD("Open file:%d", m_fileSelector);

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOpenMode", openMode));
    // LOGD("File operation set %d", openMode);

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_OPEN));
    // LOGD("File operation set %d", FILE_OP_SEL_OPEN);

    ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
    ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
    // LOGD("File operation status: %d", op_status);

    return (static_cast<TY_FILE_OP_STATUS>(op_status) == FILE_OP_STATUS_SUCC);
}

bool FileAccessControlBuf::closeFile() {
    int64_t op_status= 0;

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_CLOSE));
    // LOGD("Close file");
    ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
    ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
    // LOGD("File operation status: %d", op_status);

    return (static_cast<TY_FILE_OP_STATUS>(op_status) == FILE_OP_STATUS_SUCC);
}

bool FileAccessControlBuf::deleteFile() {
    int64_t op_status= 0;

    m_isOpen = openFile(FILE_OPEN_MODE_WRITE);
    if (!m_isOpen) {
        return false;
    }

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_DELETE));
    ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
    ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
    if (static_cast<TY_FILE_OP_STATUS>(op_status) != FILE_OP_STATUS_SUCC) {
        LOGE("File delete failed");
        return false;
    }

    return true;
}

FileAccessControlBuf::int_type FileAccessControlBuf::underflow() {
    if (!m_isOpen) {
        return traits_type::eof();
    }
    
    // read data from file to buffer
    size_t bytesRead = readFromFile(&m_buffer.front(), m_bufferSize);
    if (bytesRead == 0) {
        return traits_type::eof();
    }
    
    // set input buffer
    char* base = &m_buffer.front();
    setg(base, base, base + bytesRead);
    
    return traits_type::to_int_type(*gptr());
}

FileAccessControlBuf::int_type FileAccessControlBuf::overflow(int_type ch) {
    if (!m_isOpen) {
        return traits_type::eof();
    }
    
    // sync data in buffer
    if (sync() == -1) {
        return traits_type::eof();
    }
    
    // handle extra data
    if (ch != traits_type::eof()) {
        *pptr() = traits_type::to_char_type(ch);
        pbump(1);
    }
    
    return ch;
}

int FileAccessControlBuf::sync() {
    if (!m_isOpen) {
        return 0;
    }
    
    // write data in buffer to remove device
    if (pbase() != pptr()) {
        size_t bytesToWrite = pptr() - pbase();
        size_t bytesWritten = writeToFile(pbase(), bytesToWrite);
        if (bytesWritten != bytesToWrite) {
            return -1;
        }
        
        // reset buffer
        setp(pbase(), epptr());
    }
    
    return 0;
}

std::streambuf::pos_type FileAccessControlBuf::seekoff(
    off_type off, std::ios_base::seekdir way, std::ios_base::openmode which) {
    
    if (!m_isOpen) {
        return pos_type(off_type(-1));
    }
    
    // sync buffer
    sync();
    
    // calc new position
    long int newPos = 0;
    switch (way) {
        case std::ios_base::beg:
            newPos = static_cast<long int>(off);
            break;
        case std::ios_base::cur:
            newPos = static_cast<long int>(m_currentPosition + off);
            break;
        case std::ios_base::end: {
            size_t fileSize = getFileSize();
            newPos = static_cast<long int>(fileSize + off);
            break;
        }
        default:
            return pos_type(off_type(-1));
    }
    
    if (newPos < 0) {
        return pos_type(off_type(-1));
    }
    
    m_currentPosition = newPos;
    
    // reset buffer
    char* base = &m_buffer.front();
    setg(base, base, base);
    setp(base, base + m_bufferSize);
    
    return pos_type(newPos);
}

std::streambuf::pos_type FileAccessControlBuf::seekpos(
    pos_type pos, std::ios_base::openmode which) {
    return seekoff(pos, std::ios_base::beg, which);
}

size_t FileAccessControlBuf::readFromFile(void* buf, size_t count) {
    int64_t op_status= 0;
    int64_t n_read = 0;
    int64_t package_size = 0;
    int64_t max_len = 0;
    size_t total_read = 0;
    int64_t remaining = 0;

    size_t fsize = getFileSize();
    // LOGD(" fsize: %zd, count: %zd", fsize, count);

    remaining = count > (fsize - m_currentPosition) ? (fsize - m_currentPosition) : count;
    // LOGD(" remaining: %zd", remaining);

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_READ));
    // LOGD("File operation set %d", FILE_OP_SEL_READ);

    while (remaining > 0) { 
        if (seekTofile() != TY_STATUS_OK) {
            return static_cast<size_t>(total_read);
        }

        ASSERT_OK(TYIntegerGetMax(m_hDevice, "FileAccessLength", &max_len));

        package_size = (remaining > max_len) ? max_len : remaining;
    
        ASSERT_OK(TYIntegerSetValue(m_hDevice, "FileAccessLength", package_size));
        // LOGD("FileAccessLength: %d", count);

        ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
        ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
        // LOGD("File operation status: %d", op_status);
        if (static_cast<TY_FILE_OP_STATUS>(op_status) != FILE_OP_STATUS_SUCC) {
            LOGE("File read failed");
            return static_cast<size_t>(total_read);
        }    

        ASSERT_OK(TYIntegerGetValue(m_hDevice, "FileOperationResult", &n_read));
        // LOGD("%d bytes read", n_read);
        if (n_read <= 0) {
            break;
        }

        ASSERT_OK(TYByteArrayGetValue(m_hDevice, "FileAccessBuffer", reinterpret_cast<uint8_t*>(static_cast<char*>(buf) + total_read), n_read));

        remaining = remaining - n_read;
        m_currentPosition += n_read;
        total_read += n_read;
    }

    return static_cast<size_t>(total_read);
}

size_t FileAccessControlBuf::writeToFile(const void* buf, size_t count) {
    int64_t op_status= 0;
    int64_t n_write = 0;
    int64_t package_size = 0;
    int64_t max_len = 0;
    size_t total_written = 0;
    int64_t remaining = count;

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_WRITE));
    // LOGD("File operation set %d", FILE_OP_SEL_WRITE);

    while(remaining > 0) {
        if (seekTofile() != TY_STATUS_OK) {
            return static_cast<size_t>(total_written);
        }

        ASSERT_OK(TYIntegerGetMax(m_hDevice, "FileAccessLength", &max_len));

        package_size = (remaining > max_len) ? max_len : remaining;
    
        ASSERT_OK(TYIntegerSetValue(m_hDevice, "FileAccessLength", package_size));
        // LOGD("FileAccessLength: %d", count);
        ASSERT_OK(TYByteArraySetValue(m_hDevice, "FileAccessBuffer", reinterpret_cast<const uint8_t*>(static_cast<const char*>(buf) + m_currentPosition), package_size));
        ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
        ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
        // LOGD("File operation status: %d", op_status);
        if (static_cast<TY_FILE_OP_STATUS>(op_status) != FILE_OP_STATUS_SUCC) {
            LOGE("File write failed");
            return static_cast<size_t>(total_written);
        }
        ASSERT_OK(TYIntegerGetValue(m_hDevice, "FileOperationResult", &n_write));
        // LOGD("%d bytes write", n_write);
        if (n_write <= 0) {
            break;
        }

        remaining = remaining - n_write;
        m_currentPosition += n_write;
        total_written += n_write;
    }

    return static_cast<size_t>(total_written);
}

TY_STATUS FileAccessControlBuf::seekTofile() {
    // LOGD("FileAccessOffset set: %zd", m_currentPosition);
    return TYIntegerSetValue(m_hDevice, "FileAccessOffset", m_currentPosition);
}

size_t FileAccessControlBuf::getFileSize() {
    int64_t fsize = 0;
    ASSERT_OK(TYIntegerGetValue(m_hDevice, "FileSize", &fsize));
    return static_cast<size_t>(fsize);
}



// FileAccessControl
FileAccessControl::FileAccessControl(TY_DEV_HANDLE hDevice, size_t bufferSize)
    : std::iostream(&m_buf)
    , m_buf(hDevice, bufferSize) {
}

bool FileAccessControl::open(const char *file_name, std::ios::openmode mode) {
    clear();
    return m_buf.open(file_name, mode);
}

bool FileAccessControl::close() {
    return m_buf.close();
}

bool FileAccessControl::isOpen() const {
    return m_buf.isOpen();
}

void FileAccessControl::setFileSelector(const char *fileSel) {
    m_buf.setFileSelector(fileSel);
}

const std::string& FileAccessControl::getFileSelector() const {
    return m_buf.getFileSelector();
}

FileAccessControlBuf* FileAccessControl::rdbuf() const {
    return const_cast<FileAccessControlBuf*>(&m_buf);
}

} // namespace percipio
