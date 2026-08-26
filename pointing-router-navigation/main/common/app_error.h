#pragma once

enum class AppStatus {
    kOk = 0,
    kIgnored,
    kInvalidArgument,
    kInvalidCluster,
    kInvalidCommand,
    kInvalidPayload,
    kResponseDisabled,
    kNetworkNotReady,
    kQueueFull,
    kDriverError,
};
