#include "lead_manager/http_client.hpp"

#include <windows.h>
#include <winhttp.h>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace lead_manager {

namespace {

struct HandleCloser {
  void operator()(HINTERNET handle) const {
    if (handle != nullptr) {
      WinHttpCloseHandle(handle);
    }
  }
};

using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HINTERNET>, HandleCloser>;

std::wstring utf8ToWide(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
  if (length <= 0) {
    throw std::runtime_error("Failed to convert UTF-8 string to wide string.");
  }
  std::wstring result(length, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), length);
  return result;
}

std::string wideToUtf8(const std::wstring& value) {
  if (value.empty()) {
    return {};
  }
  const int length =
      WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (length <= 0) {
    throw std::runtime_error("Failed to convert wide string to UTF-8.");
  }
  std::string result(length, '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), result.data(), length, nullptr,
                      nullptr);
  return result;
}

struct ParsedUrl {
  bool secure = true;
  INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
  std::wstring host;
  std::wstring pathAndQuery;
};

ParsedUrl parseUrl(const std::string& url) {
  std::wstring wideUrl = utf8ToWide(url);
  URL_COMPONENTS components{};
  components.dwStructSize = sizeof(components);

  wchar_t hostBuffer[512];
  wchar_t pathBuffer[4096];
  components.lpszHostName = hostBuffer;
  components.dwHostNameLength = static_cast<DWORD>(std::size(hostBuffer));
  components.lpszUrlPath = pathBuffer;
  components.dwUrlPathLength = static_cast<DWORD>(std::size(pathBuffer));
  components.lpszExtraInfo = pathBuffer + 2048;
  components.dwExtraInfoLength = 2047;

  if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
    throw std::runtime_error("Failed to parse URL: " + url);
  }

  ParsedUrl parsed;
  parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
  parsed.port = components.nPort;
  parsed.host.assign(components.lpszHostName, components.dwHostNameLength);

  std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
  std::wstring extra(components.lpszExtraInfo, components.dwExtraInfoLength);
  parsed.pathAndQuery = path + extra;
  if (parsed.pathAndQuery.empty()) {
    parsed.pathAndQuery = L"/";
  }

  return parsed;
}

std::string lastErrorMessage(const std::string& prefix) {
  const DWORD errorCode = GetLastError();
  std::ostringstream stream;
  stream << prefix << " (Win32 error " << errorCode << ")";
  return stream.str();
}

}  // namespace

HttpResponse HttpClient::request(const std::string& method,
                                 const std::string& url,
                                 const std::map<std::string, std::string>& headers,
                                 const std::string& body,
                                 const std::string& contentType) const {
  try {
    const ParsedUrl parsed = parseUrl(url);

    UniqueHandle session(WinHttpOpen(L"gpt-cpp-lead-manager/1.0",
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS,
                                     0));
    if (!session) {
      return {.error = lastErrorMessage("Failed to open WinHTTP session")};
    }

    WinHttpSetTimeouts(session.get(), 30000, 30000, 30000, 60000);

    UniqueHandle connection(WinHttpConnect(session.get(), parsed.host.c_str(), parsed.port, 0));
    if (!connection) {
      return {.error = lastErrorMessage("Failed to connect WinHTTP session")};
    }

    const DWORD requestFlags = parsed.secure ? WINHTTP_FLAG_SECURE : 0;
    const std::wstring wideMethod = utf8ToWide(method);
    UniqueHandle requestHandle(WinHttpOpenRequest(connection.get(),
                                                  wideMethod.c_str(),
                                                  parsed.pathAndQuery.c_str(),
                                                  nullptr,
                                                  WINHTTP_NO_REFERER,
                                                  WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                  requestFlags));
    if (!requestHandle) {
      return {.error = lastErrorMessage("Failed to open WinHTTP request")};
    }

    std::wstring headerBlock;
    if (!body.empty() && !contentType.empty()) {
      headerBlock += utf8ToWide("Content-Type: " + contentType + "\r\n");
    }
    for (const auto& [name, value] : headers) {
      headerBlock += utf8ToWide(name + ": " + value + "\r\n");
    }

    const LPVOID bodyPointer = body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data());
    const DWORD bodySize = static_cast<DWORD>(body.size());
    if (!WinHttpSendRequest(requestHandle.get(),
                            headerBlock.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerBlock.c_str(),
                            headerBlock.empty() ? 0 : static_cast<DWORD>(headerBlock.size()),
                            bodyPointer,
                            bodySize,
                            bodySize,
                            0)) {
      return {.error = lastErrorMessage("Failed to send WinHTTP request")};
    }

    if (!WinHttpReceiveResponse(requestHandle.get(), nullptr)) {
      return {.error = lastErrorMessage("Failed to receive WinHTTP response")};
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(requestHandle.get(),
                        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX,
                        &statusCode,
                        &statusCodeSize,
                        WINHTTP_NO_HEADER_INDEX);

    std::string responseBody;
    for (;;) {
      DWORD bytesAvailable = 0;
      if (!WinHttpQueryDataAvailable(requestHandle.get(), &bytesAvailable)) {
        return {.error = lastErrorMessage("Failed to query WinHTTP response length")};
      }
      if (bytesAvailable == 0) {
        break;
      }

      std::vector<char> buffer(bytesAvailable);
      DWORD bytesRead = 0;
      if (!WinHttpReadData(requestHandle.get(), buffer.data(), bytesAvailable, &bytesRead)) {
        return {.error = lastErrorMessage("Failed to read WinHTTP response body")};
      }
      responseBody.append(buffer.data(), bytesRead);
    }

    return {.status = static_cast<int>(statusCode), .body = responseBody};
  } catch (const std::exception& exception) {
    return {.error = exception.what()};
  }
}

}  // namespace lead_manager

