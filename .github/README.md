# LimitlessOS GitHub Configuration

This directory contains GitHub-specific configuration files for LimitlessOS.

## Files

### `copilot-instructions.md`

Comprehensive instructions for GitHub Copilot to assist with LimitlessOS development. These instructions guide AI-assisted code generation for:

- **Microkernel Architecture**: Core kernel components (IPC, scheduler, memory manager)
- **Hardware Abstraction Layer (HAL)**: Driver interfaces and implementations
- **Persona Framework**: Windows/Linux/macOS compatibility layers
- **AI Integration**: Telemetry, anomaly detection, and optimization hooks
- **Driver Development**: USB, PCIe, NVMe, GPU, networking drivers
- **Build System**: Makefile patterns and CI/CD workflows
- **Documentation**: API references, architecture diagrams

### `workflows/`

GitHub Actions workflows for continuous integration and deployment:

- **build-and-test.yml**: Main CI workflow that builds all components, runs tests, and performs quality checks

## Using Copilot Instructions

GitHub Copilot will automatically read `copilot-instructions.md` when you're working in this repository. The instructions provide:

1. **Architecture Patterns**: How to structure microkernel components
2. **Code Templates**: Scaffolding for drivers, IPC, scheduling, etc.
3. **Best Practices**: Security, performance, and coding standards
4. **API Guidelines**: Naming conventions and function signatures
5. **Testing Patterns**: Unit and integration test examples
6. **Documentation Standards**: Doxygen comments and architecture diagrams

## Development Workflow

1. **Write code** with Copilot assistance following the instructions
2. **Build locally** with `make all`
3. **Test locally** with QEMU or VirtualBox
4. **Push changes** - CI will automatically build and test
5. **Review results** - Check GitHub Actions for build status

## Phase Development

Follow the roadmap in `../ROADMAP.md`:

- **Phase 1**: Critical Foundation (USB, filesystems, ACPI, SMP, NVMe)
- **Phase 2**: Core OS Expansion (VFS, AI scheduler, personas, sandboxing)
- **Phase 3**: Desktop & Usability (GUI, natural language control, app store)
- **Phase 4**: Security & Reliability (formal verification, encryption, intrusion detection)
- **Phase 5**: Innovation & Beyond (AI assistant, quantum APIs, AR/VR, distributed mesh)

## Contributing

When contributing to LimitlessOS:

1. Read `copilot-instructions.md` to understand the architecture
2. Follow the coding standards and patterns
3. Add tests for new features
4. Update documentation
5. Ensure CI passes

## Resources

- Main documentation: `../docs/`
- Build instructions: `../docs/BUILD.md`
- Architecture overview: `../docs/ARCHITECTURE.md`
- Development roadmap: `../ROADMAP.md`
- Gap analysis: `../GAP_ANALYSIS.md`

---

**LimitlessOS**: The future of operating systems - universal, fast, secure, intelligent, beautiful, and scalable.
