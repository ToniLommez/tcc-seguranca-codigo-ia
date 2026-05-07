#include "http_client.hpp"

#include <stdexcept>
#include <string>

#include <windows.h>
#include <winhttp.h>

#include "utils.hpp"

namespace {

struct ParsedUrl {
    std::wstring host;
    std::wstring pathAndQuery;
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;
    bool secure = true;
};

ParsedUrl parse_url(const std::string& url) {
    ParsedUrl parsed;
    const auto wideUrl = utf8_to_wide(url);

    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(wideUrl.c_str(), 0, 0, &components)) {
        throw std::runtime_error("Nao foi possivel interpretar a URL: " + url);
    }

    parsed.host.assign(components.lpszHostName, components.dwHostNameLength);
    parsed.port = components.nPort;
    parsed.secure = components.nScheme == INTERNET_SCHEME_HTTPS;
    parsed.pathAndQuery.assign(components.lpszUrlPath, components.dwUrlPathLength);
    if (components.dwExtraInfoLength > 0) {
        parsed.pathAndQuery.append(components.lpszExtraInfo, components.dwExtraInfoLength);
    }
    if (parsed.pathAndQuery.empty()) {
        parsed.pathAndQuery = L"/";
    }
    return parsed;
}

std::wstring build_headers(const std::map<std::string, std::string>& headers) {
    std::wstring joined;
    for (const auto& [name, value] : headers) {
        joined += utf8_to_wide(name + ": " + value + "\r\n");
    }
    return joined;
}

}  // namespace

HttpResponse http_request(
    const std::string& method,
    const std::string& url,
    const std::map<std::string, std::string>& headers,
    const std::string& body
) {
    const auto parsed = parse_url(url);

    HINTERNET session = WinHttpOpen(L"LeadManagerCpp/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        throw std::runtime_error("Falha ao abrir sessao WinHTTP");
    }

    HINTERNET connection = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
    if (!connection) {
        WinHttpCloseHandle(session);
        throw std::runtime_error("Falha ao conectar em " + url);
    }

    const auto methodWide = utf8_to_wide(method);
    HINTERNET request = WinHttpOpenRequest(
        connection,
        methodWide.c_str(),
        parsed.pathAndQuery.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        parsed.secure ? WINHTTP_FLAG_SECURE : 0
    );
    if (!request) {
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        throw std::runtime_error("Falha ao abrir request WinHTTP");
    }

    const auto headerText = build_headers(headers);
    if (!headerText.empty()) {
        WinHttpAddRequestHeaders(request, headerText.c_str(), static_cast<DWORD>(headerText.size()), WINHTTP_ADDREQ_FLAG_ADD);
    }

    if (!WinHttpSendRequest(
            request,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
            static_cast<DWORD>(body.size()),
            static_cast<DWORD>(body.size()),
            0)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        throw std::runtime_error("Falha ao enviar request HTTP");
    }

    if (!WinHttpReceiveResponse(request, nullptr)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        throw std::runtime_error("Falha ao receber resposta HTTP");
    }

    DWORD statusCode = 0;
    DWORD statusLength = sizeof(statusCode);
    if (!WinHttpQueryHeaders(
            request,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusLength,
            WINHTTP_NO_HEADER_INDEX)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connection);
        WinHttpCloseHandle(session);
        throw std::runtime_error("Falha ao consultar status HTTP");
    }

    std::string responseBody;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(request, &available) || available == 0) {
            break;
        }

        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read)) {
            break;
        }
        chunk.resize(read);
        responseBody += chunk;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);

    return HttpResponse{static_cast<int>(statusCode), responseBody, {}};
}
