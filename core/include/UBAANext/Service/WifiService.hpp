#pragma once

#include <UBAANext/Base/Result.hpp>
#include <UBAANext/Crypto/CryptoProvider.hpp>
#include <UBAANext/Model/FeatureRecord.hpp>
#include <UBAANext/Model/Wifi.hpp>
#include <UBAANext/Net/HttpClient.hpp>
#include <UBAANext/Net/NetworkEnvironment.hpp>
#include <UBAANext/Service/WriteOperationGate.hpp>

#include <string>

namespace UBAANext {

class WifiService {
public:
    WifiService(IHttpClient &http_client,
                INetworkEnvironment *network_environment,
                ICryptoProvider &crypto_provider,
                Model::WifiCredentials credentials = {});

    void set_write_operation_gate(WriteOperationGate gate);

    /** WriteGated remote mutation: logs in to BUAA gateway; fails closed outside campus network. */
    Result<Model::MutationResult> login();
    /** WriteGated remote mutation: logs out from BUAA gateway; fails closed outside campus network. */
    Result<Model::MutationResult> logout();

private:
    IHttpClient &m_http_client;
    INetworkEnvironment *m_network_environment = nullptr;
    ICryptoProvider &m_crypto_provider;
    Model::WifiCredentials m_credentials;
    WriteOperationGate m_write_gate = disabled_write_operation("wifi");

    Result<Model::WifiResult> execute(const std::string &action);
};

} // namespace UBAANext
