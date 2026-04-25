"""Custom exceptions for the registry."""


class RegistryError(Exception):
    """Base class for registry exceptions."""


class FunctionNotFoundError(RegistryError):
    """Raised when looking up a function that isn't registered."""


class FunctionAlreadyExistsError(RegistryError):
    """Raised when registering a function with an ID that's already taken."""


class FunctionInUseError(RegistryError):
    """Raised when trying to unregister a function still referenced by others.

    The error message should include the set of referrers (composite
    function IDs that depend on this one) so users know what to remove first.
    """
