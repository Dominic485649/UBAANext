"""autoTD command line package."""

__version__ = "0.1.15"

# Expose submodule for test-time patching and simple interactive imports.
from . import quick as quick  # noqa: E402,F401
