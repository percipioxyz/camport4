// FileAccessControl.cpp
#include "FileAccessControl.hpp"
// #include "TYFileAccessController.hpp"
#include "Utils.hpp"
#include <cstring>

namespace percipio {

FileAccessControlBuf::FileAccessControlBuf(TY_DEV_HANDLE hDevice, size_t bufferSize)
    : m_hDevice(hDevice)
    , m_fileSelector(static_cast<TY_FILE_SEL>(0))
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

bool FileAccessControlBuf::open(TY_FILE_SEL fileSel, std::ios::openmode mode) {
    m_fileSelector = fileSel;
    
    TY_FILE_OPEN_MODE openMode;
    if ((mode & std::ios::out) && (mode & std::ios::in)) {
        openMode = FILE_OPEN_MODE_READWRITE;
    } else if (mode & std::ios::out) {
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
    int32_t op_status= 0;
    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileSelector", m_fileSelector));
    // LOGD("Open file:%d", m_fileSelector);

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOpenMode", openMode));
    // LOGD("File operation set %d", openMode);

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_OPEN));
    // LOGD("File operation set %d", FILE_OP_SEL_OPEN);

    ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
    ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
    // LOGD("File operation status: %d", op_status);

    return (op_status == FILE_OP_STATUS_SUCC);
}

bool FileAccessControlBuf::closeFile() {
    int32_t op_status= 0;

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_CLOSE));
    // LOGD("Close file");
    ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
    ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
    // LOGD("File operation status: %d", op_status);

    return (op_status == FILE_OP_STATUS_SUCC);
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
    int32_t op_status= 0;
    int64_t n_read = 0;

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_READ));
    // LOGD("File operation set %d", FILE_OP_SEL_READ);

    if (seekTofile() != TY_STATUS_OK) {
        return static_cast<size_t>(0);
    }

    size_t fsize = getFileSize();

    ASSERT_OK(TYIntegerSetValue(m_hDevice, "FileAccessLength", fsize > count ? count : fsize));
    // LOGD("FileAccessLength: %zd", fsize > count ? count : fsize);

    ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));

    ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
    // LOGD("File operation status:%d", op_status);
    if (op_status != FILE_OP_STATUS_SUCC) {
        LOGE("File read failed");
        return static_cast<size_t>(0);
    }
    
    ASSERT_OK(TYIntegerGetValue(m_hDevice, "FileOperationResult", &n_read));
    // LOGD("%d bytes read", n_read);

    ASSERT_OK(TYByteArrayGetValue(m_hDevice, "FileAccessBuffer", reinterpret_cast<uint8_t*>(buf), n_read));

    m_currentPosition += n_read;

    return static_cast<size_t>(n_read);
}

size_t FileAccessControlBuf::writeToFile(const void* buf, size_t count) {
    int32_t op_status= 0;
    int64_t n_write = 0;

    ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_WRITE));
    // LOGD("File operation set %d", FILE_OP_SEL_WRITE);

    if (seekTofile() != TY_STATUS_OK) {
        return static_cast<size_t>(0);
    }

    size_t fsize = getFileSize();

    // if remote file size is larger than local file size, remove the remote file to make sure the remote file can be rewritten
    if (fsize > count) {
        ASSERT_OK(TYEnumSetValue(m_hDevice, "FileOperationSelector", FILE_OP_SEL_DELETE));
        ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
    }

    ASSERT_OK(TYIntegerSetValue(m_hDevice, "FileAccessLength", count));
    // LOGD("FileAccessLength: %d", count);
    ASSERT_OK(TYByteArraySetValue(m_hDevice, "FileAccessBuffer", reinterpret_cast<const uint8_t*>(buf), count));
    ASSERT_OK(TYCommandExec(m_hDevice, "FileOperationExecute"));
    ASSERT_OK(TYEnumGetValue(m_hDevice, "FileOperationStatus", &op_status));
    // LOGD("File operation status: %d", op_status);
    if (op_status != FILE_OP_STATUS_SUCC) {
        LOGE("File read failed");
        return static_cast<size_t>(0);
    }

    ASSERT_OK(TYIntegerGetValue(m_hDevice, "FileOperationResult", &n_write));
    // LOGD("%d bytes write", n_write);

    m_currentPosition += n_write;

    return static_cast<size_t>(n_write);
}

TY_STATUS FileAccessControlBuf::seekTofile() {
    return TYIntegerSetValue(m_hDevice, "FileAccessOffset", m_currentPosition);
}

size_t FileAccessControlBuf::getFileSize() {
    int64_t fsize = 0;
    ASSERT_OK(TYIntegerGetValue(m_hDevice, "FileSize", &fsize));
    return static_cast<size_t>(fsize);
}



// FileAccessControl 实现
FileAccessControl::FileAccessControl(TY_DEV_HANDLE hDevice, size_t bufferSize)
    : std::iostream(&m_buf)
    , m_buf(hDevice, bufferSize) {
}

bool FileAccessControl::open(TY_FILE_SEL fileSel, std::ios::openmode mode) {
    return m_buf.open(fileSel, mode);
}

bool FileAccessControl::close() {
    return m_buf.close();
}

bool FileAccessControl::isOpen() const {
    return m_buf.isOpen();
}

void FileAccessControl::setFileSelector(TY_FILE_SEL fileSel) {
    m_buf.setFileSelector(fileSel);
}

TY_FILE_SEL FileAccessControl::getFileSelector() const {
    return m_buf.getFileSelector();
}

FileAccessControlBuf* FileAccessControl::rdbuf() const {
    return const_cast<FileAccessControlBuf*>(&m_buf);
}

} // namespace percipio
