namespace automation::api {

static std::optional<bool> HandleJobsV2Command(
    const std::string& cmd,
    const std::map<std::string, std::string>& params,
    const std::string& requestId,
    const std::function<bool(const std::string& msg,
                             const std::string& jsonKey,
                             int responseCode,
                             bool msgIsJSON)>& sendResponse) {
    if (cmd == "jobs.get") {
        std::string jobId = ReadParamString(params, "jobId");
        if (jobId.empty() || jobId == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "jobId is required.", requestId), "", 422, true);
        }
        V2JobRecord* job = FindV2Job(jobId);
        if (job == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "JOB_NOT_FOUND", "Job not found.", requestId), "", 404, true);
        }
        return sendResponse(BuildV2SuccessResponse(200, cmd, BuildV2JobData(*job), requestId), "", 200, true);
    }

    if (cmd == "jobs.cancel") {
        std::string jobId = ReadParamString(params, "jobId");
        if (jobId.empty() || jobId == "null") {
            return sendResponse(BuildV2ErrorResponse(422, cmd, "VALIDATION_ERROR", "jobId is required.", requestId), "", 422, true);
        }
        V2JobRecord* job = FindV2Job(jobId);
        if (job == nullptr) {
            return sendResponse(BuildV2ErrorResponse(404, cmd, "JOB_NOT_FOUND", "Job not found.", requestId), "", 404, true);
        }

        std::string reason;
        bool cancelled = MarkV2JobCancelled(jobId, reason);
        nlohmann::json data = BuildV2JobData(*job);
        data["cancelled"] = cancelled;
        data["cancelReason"] = reason;
        return sendResponse(BuildV2SuccessResponse(200, cmd, data, requestId), "", 200, true);
    }

    return std::nullopt;
}

} // namespace automation::api
