#!/usr/bin/env python3
"""
Go gRPC service generator — generate Go gRPC server implementation from proto definition.
Input: .proto service definition
Output: Go gRPC server struct implementation
"""
import sys
import json
import os

METADATA = {
    "name": "go_grpc_gen",
    "description": "Generate Go gRPC server implementation from a proto service definition",
    "version": "1.0",
    "tags": ["go", "golang", "grpc", "proto", "codegen"],
    "input": "protobuf service definition",
    "output": "go grpc server implementation",
}

SYSTEM_PROMPT = """You are an expert Go gRPC engineer.
Generate a Go gRPC server struct implementing the given proto service.
Requirements:
- Implement all RPC methods with correct signatures
- Use context properly (check ctx.Done(), propagate cancellation)
- Return proper gRPC status codes (codes.NotFound, codes.InvalidArgument, etc.)
- Add a constructor New<ServiceName>Server(deps...) *<ServiceName>Server
- Use interface deps (not concrete types) for storage/business logic
Output only the Go implementation file."""


def run(proto: str) -> str:
    import litellm
    model = os.environ.get("LLM_MODEL", "gemini/gemini-2.5-flash")
    response = litellm.completion(
        model=model,
        messages=[
            {"role": "system", "content": SYSTEM_PROMPT},
            {"role": "user", "content": proto},
        ],
    )
    return response.choices[0].message.content


if __name__ == "__main__":
    if "--metadata" in sys.argv:
        print(json.dumps(METADATA))
    else:
        text = sys.argv[1] if len(sys.argv) > 1 else sys.stdin.read()
        print(run(text))
