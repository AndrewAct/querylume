#include "querylume/common/error.h"

namespace querylume {

std::string toString(ErrorCode code) {
    switch (code) {
        case ErrorCode::kJsonParseError:
            return "JsonParseError";
        case ErrorCode::kInvalidDataShape:
            return "InvalidDataShape";
        case ErrorCode::kUnsupportedValueType:
            return "UnsupportedValueType";
        case ErrorCode::kPipelineSyntaxError:
            return "PipelineSyntaxError";
        case ErrorCode::kUnknownStage:
            return "UnknownStage";
        case ErrorCode::kUnknownField:
            return "UnknownField";
        case ErrorCode::kUnsupportedOperator:
            return "UnsupportedOperator";
        case ErrorCode::kIncompatibleTypes:
            return "IncompatibleTypes";
        case ErrorCode::kInvalidSortDirection:
            return "InvalidSortDirection";
        case ErrorCode::kInvalidLimit:
            return "InvalidLimit";
        case ErrorCode::kInvalidStageLifecycle:
            return "InvalidStageLifecycle";
        case ErrorCode::kUnsupportedLogicalNode:
            return "UnsupportedLogicalNode";
    }
    return "UnknownError";
}

}  // namespace querylume
