/**
 * Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
 *
 * See file LICENSE for terms.
 */
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>

#include <ucp/api/ucp.h>

#include <ucxx/constructors.h>
#include <ucxx/component.h>
#include <ucxx/config.h>
#include <ucxx/utils.h>
//#include <ucxx/worker.h>

namespace ucxx
{


class UCXXWorker;


class UCXXContext : public UCXXComponent
{
    private:
        ucp_context_h _handle{nullptr};
        UCPConfig _config{};
        uint64_t _feature_flags{0};
        bool _cuda_support{false};

    public:
    static constexpr uint64_t default_feature_flags = UCP_FEATURE_TAG | UCP_FEATURE_WAKEUP | UCP_FEATURE_STREAM | UCP_FEATURE_AM | UCP_FEATURE_RMA;

    UCXXContext() = default;

    UCXXContext(std::map<std::string, std::string> ucx_config, uint64_t feature_flags) : _config{UCPConfig(ucx_config)}, _feature_flags{feature_flags}
    {
        ucp_params_t ucp_params;

        // UCP
        std::memset(&ucp_params, 0, sizeof(ucp_params));
        ucp_params.field_mask = (
            UCP_PARAM_FIELD_FEATURES |
            UCP_PARAM_FIELD_REQUEST_SIZE |
            UCP_PARAM_FIELD_REQUEST_INIT
        );
        ucp_params.features = feature_flags;
        ucp_params.request_size = sizeof(ucxx::ucxx_request_t);
        ucp_params.request_init = ucx_py_request_reset;

        assert_ucs_status(ucp_init(&ucp_params, this->_config.get_handle(), &this->_handle));

        // UCX supports CUDA if "cuda" is part of the TLS or TLS is "all"
        auto config_map = this->_config.get();
        auto tls = config_map.find("TLS");
        if (tls != config_map.end())
            this->_cuda_support = tls->second == "all" || tls->second.find("cuda") != std::string::npos;

        std::cout << "UCP initiated using config: " << std::endl;
        for (const auto& kv : config_map)
            std::cout << "  " << kv.first << ": " << kv.second << std::endl;
    }

    UCXXContext(const UCXXContext&) = delete;
    UCXXContext& operator=(UCXXContext const&) = delete;

    UCXXContext(UCXXContext&& o) noexcept
        : _handle{std::exchange(o._handle, nullptr)},
          _config{std::exchange(o._config, {})},
          _feature_flags{std::exchange(o._feature_flags, 0)},
          _cuda_support{std::exchange(o._cuda_support, false)}
    {
    }

    UCXXContext& operator=(UCXXContext&& o) noexcept
    {
        this->_handle = std::exchange(o._handle, nullptr);
        this->_config = std::exchange(o._config, {});
        this->_feature_flags = std::exchange(o._feature_flags, 0);
        this->_cuda_support = std::exchange(o._cuda_support, false);

        return *this;
    }

    static std::shared_ptr<UCXXContext> create(std::map<std::string, std::string> ucx_config, uint64_t feature_flags)
    {
        // TODO: make constructor private, probably using pImpl
        return std::make_shared<UCXXContext>(ucx_config, feature_flags);
    }

    ~UCXXContext()
    {
        if (this->_handle != nullptr)
            ucp_cleanup(this->_handle);
    }

    std::map<std::string, std::string> get_config()
    {
        return this->_config.get();
    }

    ucp_context_h get_handle()
    {
        assert(this->initialized);
        return this->_handle;
    }

    std::string get_info()
    {
        assert(this->initialized);

        FILE *text_fd = create_text_fd();
        ucp_context_print_info(this->_handle, text_fd);
        return decode_text_fd(text_fd);
    }

    uint64_t get_feature_flags() const
    {
        return _feature_flags;
    }

    std::shared_ptr<UCXXWorker> createWorker()
    {
        auto context = std::dynamic_pointer_cast<UCXXContext>(shared_from_this());
        auto worker = ucxx::createWorker(context);
        addChild(std::dynamic_pointer_cast<UCXXComponent>(worker));
        return worker;
    }
};

} // namespace ucxx
