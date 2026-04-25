"""Tests for LLMFunction."""

from unittest.mock import MagicMock

from ai_core.functions.llm import LLMFunction


class TestConstruction:
    def test_stores_closure(self) -> None:
        f = LLMFunction("chat_v1", model="claude-sonnet-4-6", system_prompt="hi")
        assert f.id == "chat_v1"
        assert f.closure["model"] == "claude-sonnet-4-6"
        assert f.closure["system_prompt"] == "hi"
        assert f.closure["temperature"] == 0.7
        assert f.closure["max_tokens"] == 2048

    def test_metadata_type_is_llm(self) -> None:
        f = LLMFunction("chat_v1", model="claude-sonnet-4-6")
        assert f.metadata["type"] == "llm"


class TestCall:
    def test_calls_llm_client_with_messages(self, mock_llm_client: MagicMock) -> None:
        """The function should call llm_client.call() with assembled messages."""
        f = LLMFunction("chat_v1", model="claude-sonnet-4-6")
        mock_llm_client.call.return_value = "hello back"

        out, ctx = f(b"hi", {})

        assert out == b"hello back"
        mock_llm_client.call.assert_called_once()

    def test_appends_to_history(self, mock_llm_client: MagicMock) -> None:
        """After a call, llm_history should contain the user + assistant turn."""
        f = LLMFunction("chat_v1", model="claude-sonnet-4-6")
        mock_llm_client.call.return_value = "reply"

        _, ctx = f(b"hello", {})

        history = ctx["llm_history"]
        assert any(m["role"] == "user" and m["content"] == "hello" for m in history)
        assert any(m["role"] == "assistant" and m["content"] == "reply" for m in history)

    def test_includes_system_prompt_on_empty_history(
        self, mock_llm_client: MagicMock
    ) -> None:
        """If system_prompt is set and history is empty, it should be the first message."""
        f = LLMFunction("chat_v1", model="claude-sonnet-4-6", system_prompt="be brief")
        mock_llm_client.call.return_value = "ok"

        f(b"hi", {})

        sent_messages = mock_llm_client.call.call_args.kwargs["messages"]
        assert sent_messages[0]["role"] == "system"
        assert sent_messages[0]["content"] == "be brief"

    def test_preserves_existing_history(self, mock_llm_client: MagicMock) -> None:
        """Existing llm_history in context should be passed to the LLM."""
        f = LLMFunction("chat_v1", model="claude-sonnet-4-6")
        mock_llm_client.call.return_value = "third"

        history = [
            {"role": "user", "content": "first"},
            {"role": "assistant", "content": "second"},
        ]
        f(b"third user", {"llm_history": list(history)})

        sent = mock_llm_client.call.call_args.kwargs["messages"]
        assert sent[0]["content"] == "first"
        assert sent[1]["content"] == "second"
        assert sent[-1]["content"] == "third user"


class TestSerialization:
    def test_round_trip(self) -> None:
        """to_dict() -> from_dict() should yield an equivalent function."""
        f = LLMFunction("chat_v1", model="claude-sonnet-4-6", system_prompt="hi")
        data = f.to_dict()
        restored = LLMFunction.from_dict(data)
        assert restored.id == f.id
        assert restored.closure == f.closure
        assert restored.metadata == f.metadata

    def test_to_dict_includes_type(self) -> None:
        f = LLMFunction("chat_v1", model="claude-sonnet-4-6")
        d = f.to_dict()
        assert d["type"] == "llm"
