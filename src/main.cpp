#include <fstream>
#include <filesystem>

#include <clang/Lex/PreprocessorOptions.h>

#include <gflags/gflags.h>

#include "dependency_analyzer.h"
#include "entity_registry.h"
#include "exporter.h"
#include "parser.h"
#include "basic_naming_convention.h"

using namespace apigen;

// file names
DEFINE_string(api_header_file, "./api.h", "Path to the API header output.");
DEFINE_string(host_header_file, "./host.h", "Path to the host header output.");
DEFINE_string(host_source_file, "./host.cpp", "Path to the host source file output.");
DEFINE_string(
	collect_source_file, "./collect.cpp",
	"Path to the auxiliary output file used to collect structure sizes and alignments."
);

DEFINE_string(
	additional_host_include, "",
	"Path to an additional include file for all host sources. Not specifying a value causes no additional "
	"#include's to be added, however it's almost certain that some need to be added."
);

// debugging
DEFINE_string(redirect_stderr, "", "The redirected stderr file name.");

// naming
DEFINE_string(api_struct_name, "api", "Name of the API structure containing function pointers.");
DEFINE_string(api_initializer_name, "api_init", "Name of the function used to initialize the API structure.");

// TODO naming convention parameters

/// Concatenates the current working directory with \p p, then emits a warning if the root of the resulting path is
/// different from that of the current working directory.
std::filesystem::path get_absolute_path(const std::filesystem::path &p) {
	static std::filesystem::path _working_dir = std::filesystem::current_path();

	std::filesystem::path fulldir = (_working_dir / p).lexically_normal();
	if (fulldir.root_name() != _working_dir.root_name()) {
		std::cerr <<
			"warning: files are on different roots (partitions). this is currently unsupported by apigen.\n"
			"note that this only affects generated #include directives (invalid ones will be generated), while other codegen "
			"features are unaffected.\n";
	}
	return fulldir;
}
/// Returns the path required if a file at \p sourceloc needs to include the file at \p included.
std::filesystem::path get_relative_include_path(
	const std::filesystem::path &included, const std::filesystem::path &sourceloc
) {
	return included.lexically_relative(sourceloc.parent_path());
}

int main(int argc, char **argv) {
	argv[0] = "clang++";
	llvm::ArrayRef<char*> args(argv, argc);
	for (auto it = args.begin(); it != args.end(); ++it) {
		if (std::strcmp(*it, "--") == 0) {
			int new_argc = static_cast<int>(it - args.begin());
			argv[new_argc] = "clang++";
			char **new_argv = argv;
			gflags::ParseCommandLineFlags(&new_argc, &new_argv, true);
			args = llvm::ArrayRef<char*>(&*it, argv + argc);
			break;
		}
		// if this loop finishes without breaking, then there's no '--' in the arguments
		// in which case all arguments will be passed to clang
	}

	// redirect stderr if necessary
	std::ofstream stderr_redirect;
	if (!FLAGS_redirect_stderr.empty()) {
		stderr_redirect.rdbuf()->pubsetbuf(nullptr, 0); // disable buffering, must be done before opening the file
		stderr_redirect.open(FLAGS_redirect_stderr, std::ios::out | std::ios::trunc);
		std::cerr.rdbuf(stderr_redirect.rdbuf());
	}

	auto invocation = clang::createInvocationFromCommandLine(args);

	invocation->getPreprocessorOpts().addMacroDef("APIGEN_ACTIVE");
	parser p(std::move(invocation));

	entity_registry reg;
	dependency_analyzer dep_analyzer;

	reg.analyzer = &dep_analyzer;

	p.parse(reg);
	dep_analyzer.analyze(reg);

	// process paths
	std::filesystem::path
		api_header = get_absolute_path(FLAGS_api_header_file),
		host_header = get_absolute_path(FLAGS_host_header_file),
		host_source = get_absolute_path(FLAGS_host_source_file),
		collect_source = get_absolute_path(FLAGS_collect_source_file);
	std::filesystem::path additional_host_include;
	if (!FLAGS_additional_host_include.empty()) {
		additional_host_include = get_absolute_path(FLAGS_additional_host_include);
	} else {
		std::cerr << "warning: no additional host includes specified.\n";
	}

	// naming convention
	basic_naming_convention naming(reg);
	naming.api_struct_name = FLAGS_api_struct_name;
	naming.api_struct_init_function_name = FLAGS_api_initializer_name;

	// export!
	exporter exp(p.get_compiler().getASTContext().getPrintingPolicy(), naming, reg);
	exp.collect_exported_entities(reg);
	{
		std::ofstream out(api_header);
		exp.export_api_header(out);
	}
	{
		std::ofstream out(host_header);
		exp.export_host_h(out);
	}
	{
		std::ofstream out(host_source);
		if (!additional_host_include.empty()) {
			out <<
				"#include \"" << get_relative_include_path(additional_host_include, host_source).string() << "\"\n";
		}
		out << "#include \"" << get_relative_include_path(host_header, host_source).string() << "\"\n";
		out << "#include \"" << get_relative_include_path(api_header, host_source).string() << "\"\n";
		exp.export_host_cpp(out);
	}
	{
		std::ofstream out(collect_source);
		if (!additional_host_include.empty()) {
			out <<
				"#include \"" <<
				get_relative_include_path(additional_host_include, collect_source).string() <<
				"\"\n";
		}
		exp.export_data_collection_cpp(out);
	}

	return 0;
}
