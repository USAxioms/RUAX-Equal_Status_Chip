# Contributing to Equal-Status Memory Controller

Thank you for your interest in contributing! This project is focused on formal specification and silicon-ready implementation of memory controllers.

## Development Setup

```bash
git clone https://github.com/yourusername/equal-status-memory-controller.git
cd equal-status-memory-controller

# Install dependencies (tool-specific)
# See docs/ for tool requirements (JasperGold, VC Formal, etc.)

# Run all tests
make test
```

## Contribution Guidelines

### Code Quality

- **RTL Code**: SystemVerilog, must pass linting
- **C Code**: C99 standard, must compile without warnings
- **Documentation**: Markdown, clear and complete

### Testing Requirements

All changes must pass:
- ✅ RTL simulation (10 test cases)
- ✅ Formal verification (15 SVA properties)
- ✅ Coverage analysis (>95% target)

```bash
make verify    # Run all verification
make test      # Run simulation tests
make formal    # Run formal verification (requires tools)
```

### Pull Request Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Run full verification: `make verify`
5. Commit with clear messages
6. Push to your fork
7. Create a Pull Request with detailed description
8. Ensure CI/CD pipelines pass
9. Wait for review and approval

### Commit Message Format

```
[TYPE] Short description

Detailed explanation if needed.

Fixes: #123
Tests: TEST_1, TEST_2
Verified: YES
```

**Types:** `[SPEC]` `[RTL]` `[SIM]` `[FORMAL]` `[DOC]` `[TEST]`

### Formal Verification

For changes to critical logic:
1. Add/update SVA properties
2. Ensure all properties can be proven
3. Include proof certificates in PR

See [docs/03_formal_verification/](docs/03_formal_verification/) for detailed procedures.

### Documentation

- Update relevant documentation in [docs/](docs/)
- Keep specification documents synchronized
- Update CHANGELOG.md with changes

## Reporting Issues

Please include:
- Clear description of the issue
- Steps to reproduce (if applicable)
- Expected vs. actual behavior
- Environment details (OS, tools, versions)

## Areas for Contribution

- 🐛 **Bug fixes** — Found an issue? Let's fix it!
- 📝 **Documentation** — Help improve clarity and coverage
- ✨ **Enhancement** — New features or optimizations
- 🧪 **Tests** — Additional test cases or coverage
- 📚 **Examples** — Usage examples or tutorials

## Questions?

- Check [README.md](README.md) for quick start
- See [QUICK_REFERENCE_CARD.txt](QUICK_REFERENCE_CARD.txt) for overview
- Review [EXECUTIVE_SUMMARY_COMPLETE_ECOSYSTEM.md](EXECUTIVE_SUMMARY_COMPLETE_ECOSYSTEM.md)

---

Thank you for contributing! Your help makes this project better. 🚀
