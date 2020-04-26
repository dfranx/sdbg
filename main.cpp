#include <stdio.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <glslang/glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <glslang/StandAlone/DirStackFileIncluder.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <cxxopts/include/cxxopts.hpp>

extern "C" {
	#include <spvm/context.h>
	#include <spvm/state.h>
	#include <spvm/ext/GLSL450.h>
}

bool Compile(std::vector<unsigned int>& outSPV, glslang::EShSource inLang, EShLanguage shaderType, const std::string& source, const std::string& entry);

std::vector<std::string> SplitString(const std::string& str, const std::string& dlm)
{
    std::vector<std::string> ret;
    size_t pos = 0, prev = 0;
    while ((pos = str.find(dlm, prev)) != std::string::npos)
    {
        ret.push_back(str.substr(prev, pos - prev));
        prev = pos + 1;
    }

    ret.push_back(str.substr(prev));
    return ret;
}

void OutputVariableValue(spvm_state_t state, spvm_result_t type, spvm_member_t mems, spvm_word mem_count, const std::string& prefix)
{
	if (!mems) return;

	bool outPrefix = false;
	bool allRec = true;
	for (spvm_word i = 0; i < mem_count; i++) {

		spvm_result_t vtype = type;
		if (mems[i].type != 0)
			vtype = spvm_state_get_type_info(state->results, &state->results[mems[i].type]);
		if (vtype->member_count > 1 && vtype->pointer != 0 && vtype->value_type != spvm_value_type_matrix)
			vtype = spvm_state_get_type_info(state->results, &state->results[vtype->pointer]);

		if (mems[i].member_count == 0) {
			if (!outPrefix && !prefix.empty()) {
				outPrefix = true;
				printf("%s = ", prefix.c_str());
			}

			if (vtype->value_type == spvm_value_type_float && type->value_bitcount <= 32)
				printf("%.2f ", mems[i].value.f);
			else if (vtype->value_type == spvm_value_type_float)
				printf("%.2lf ", mems[i].value.d);
			else
				printf("%d ", mems[i].value.s);
			allRec = false;
		}
		else {
			std::string newPrefix = prefix;
			if (type->value_type == spvm_value_type_struct)
				newPrefix += "." + std::string(type->member_name[i]);
			if (type->value_type == spvm_value_type_matrix)
				newPrefix += "[" + std::to_string(i) + "]";

			OutputVariableValue(state, vtype, mems[i].members, mems[i].member_count, newPrefix);
		}
	}
	if (!allRec) printf("\n");
}
void InputVariableValue(spvm_state_t state, spvm_result_t type, spvm_member_t mems, spvm_word mem_count, const std::string& prefix)
{
	bool outPrefix = false;
	for (spvm_word i = 0; i < mem_count; i++) {

		spvm_result_t vtype = type;
		if (mems[i].type != 0)
			vtype = spvm_state_get_type_info(state->results, &state->results[mems[i].type]);
		if (vtype->member_count > 1 && vtype->pointer != 0 && vtype->value_type != spvm_value_type_matrix)
			vtype = spvm_state_get_type_info(state->results, &state->results[vtype->pointer]);

		if (mems[i].member_count == 0) {
			std::string actPrefix = prefix;
			if (type->value_type == spvm_value_type_array)
				actPrefix += "[" + std::to_string(i) + "]";
			if (!outPrefix && !actPrefix.empty()) {
				outPrefix = true;
				printf("%s = ", actPrefix.c_str());
			}

			if (vtype->value_type == spvm_value_type_float && type->value_bitcount <= 32)
				scanf("%f", &mems[i].value.f);
			else if (vtype->value_type == spvm_value_type_float)
				scanf("%lf", &mems[i].value.d);
			else
				scanf("%d", &mems[i].value.s);
		}
		else {
			std::string newPrefix = prefix;
			if (type->value_type == spvm_value_type_struct)
				newPrefix += "." + std::string(type->member_name[i]);
			if (type->value_type == spvm_value_type_matrix || type->value_type == spvm_value_type_array)
				newPrefix += "[" + std::to_string(i) + "]";

			InputVariableValue(state, vtype, mems[i].members, mems[i].member_count, newPrefix);
		}
	}
}
void PrintAllOutputValues(spvm_state_t state)
{
	for (spvm_word i = 0; i < state->owner->bound; i++) {
		spvm_result_t ptrInfo = nullptr;
		if (state->results[i].pointer)
			ptrInfo = &state->results[state->results[i].pointer];

		if (ptrInfo && ptrInfo->storage_class == SpvStorageClassOutput && ptrInfo->value_type == spvm_value_type_pointer) {
			std::string prefix = "";
			if (state->results[i].name)
				prefix = state->results[i].name;

			spvm_result_t type_info = spvm_state_get_type_info(state->results, ptrInfo);

			OutputVariableValue(state, type_info, state->results[i].members, state->results[i].member_count, prefix);
		}
	}
}

bool is_emit_vertex_called = false, is_end_primitive_called = false;
void emit_vertex(struct spvm_state*, spvm_word)
{
	is_emit_vertex_called = true;
	// here you might store all output variables in some kind of an array for an actual debugger
}
void end_primitive(struct spvm_state*, spvm_word)
{
	is_end_primitive_called = true;
}

int main(int argc, char* argv[]) {
	std::string filename = "SimpleVS.vk", entry = "main";
	glslang::EShSource language = glslang::EShSourceGlsl;
	EShLanguage stage = EShLangVertex;

	try {
		cxxopts::Options options("sdbg", "sdbg is a CLI tool for debugging shaders");

		options.add_options()
			("c,compiler", "compiler (hlsl/glsl)", cxxopts::value<std::string>()->default_value("glsl"))
			("s,stage", "shader stage (pixel/vertex)", cxxopts::value<std::string>()->default_value("pixel"))
			("f,file", "File name", cxxopts::value<std::string>())
			("e,entry", "entry function name", cxxopts::value<std::string>()->default_value("main"));

		auto optsResult = options.parse(argc, argv);

		if (optsResult.count("compiler")) {
			std::string cmp = optsResult["compiler"].as<std::string>();
			if (cmp == "hlsl")
				language = glslang::EShSourceHlsl;
		}
		if (optsResult.count("entry"))
			entry = optsResult["entry"].as<std::string>();
		if (optsResult.count("stage")) {
			std::string stageValue = optsResult["stage"].as<std::string>();
			bool isVertex = stageValue == "vertex" || stageValue == "vert" || stageValue == "vs";
			bool isGeometry = stageValue == "geometry" || stageValue == "geom" || stageValue == "gs";
			stage = (isVertex ? EShLangVertex : (isGeometry ? EShLangGeometry : EShLangFragment));
		}
		if (optsResult.count("file"))
			filename = optsResult["file"].as<std::string>();
	} catch (const cxxopts::OptionException & e) {
		std::cout << "error parsing options: " << e.what() << std::endl;
		exit(1);
	}

	if (filename.empty()) {
		printf("Please specify a shader file that you would like to debug.\n");
		return 0;
	}


	// start glslang process
	bool glslangInit = glslang::InitializeProcess();
	if (!glslangInit) {
		printf("Failed to initialize glslang");
		return 0;
	}

	// load shader
	std::ifstream t(filename);
	std::string src((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	t.close();

	// compile shader
	std::vector<unsigned int> spirv;
	bool cres = Compile(spirv, language, stage, src, entry);
	if (!cres) {
		printf("Failed to compile the shader.\n");
		return 0;
	}

	// spirv-vm initalization
	spvm_context_t ctx = spvm_context_initialize();

	size_t spv_length = spirv.size();
	spvm_source spv = (spvm_source)spirv.data();

	// create program & state
	spvm_program_t prog = spvm_program_create(ctx, spv, spv_length);
	spvm_state_t state = spvm_state_create(prog);

	// create GLSL.std.450 extension
	spvm_ext_opcode_func* glsl_ext_data = spvm_build_glsl450_ext();
	spvm_result_t glsl_std_450 = spvm_state_get_result(state, "GLSL.std.450");
	if (glsl_std_450)
		glsl_std_450->extension = glsl_ext_data;

	// EmitVertex and EndPrimitive
	state->emit_vertex = emit_vertex;
	state->end_primitive = end_primitive;

	// list of all textures used
	std::vector<spvm_image> images;

	// inputs & uniforms
	for (spvm_word i = 0; i < prog->bound; i++) {
		spvm_result_t ptrInfo = nullptr;
		if (state->results[i].pointer)
			ptrInfo = &state->results[state->results[i].pointer];
		
		bool isInput = false;
		
		if (ptrInfo) {
			isInput = (ptrInfo->storage_class == SpvStorageClassUniform ||
				ptrInfo->storage_class == SpvStorageClassUniformConstant ||
				ptrInfo->storage_class == SpvStorageClassInput) &&
				ptrInfo->value_type == spvm_value_type_pointer;
		}

		if (isInput) {
			spvm_result_t type_info = spvm_state_get_type_info(state->results, ptrInfo);

			// textures
			if (ptrInfo->storage_class == SpvStorageClassUniformConstant &&
				(type_info->value_type == spvm_value_type_sampled_image ||
					type_info->value_type == spvm_value_type_image)) {
				printf("Please enter \"%s\" texture file path: ", state->results[i].name);
				char filepath[1024];
				scanf("%s", filepath);

				int width, height, nrChannels;
				unsigned char* data = stbi_load(filepath, &width, &height, &nrChannels, 4);
				images.push_back(spvm_image());
				spvm_image_t img = &images[images.size()-1];

				if (data == nullptr) {
					printf("Failed to load texture %s! :(\n", filepath);

					float imgData[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
					spvm_image_create(img, imgData, 1, 1, 1);
				} else {
					float* imgData = (float*)malloc(sizeof(float) * width * height * 4);

					for (int x = 0; x < width; x++) {
						for (int y = 0; y < height; y++) {
							int index = (x + width * y) * 4;

							float* mem = imgData + index;
							
							mem[0] = data[index + 0] / 255.0f;
							mem[1] = data[index + 1] / 255.0f;
							mem[2] = data[index + 2] / 255.0f;
							mem[3] = data[index + 3] / 255.0f;
						}
					}

					spvm_image_create(img, imgData, width, height, 1);
					free(imgData);
				}

				state->results[i].members[0].image_data = img;

				stbi_image_free(data);
			}
			// other variables
			else {
				std::string prefix = "";
				if (state->results[i].name)
					prefix = state->results[i].name;

				InputVariableValue(state, type_info, state->results[i].members, state->results[i].member_count, prefix);
			}
		}
	}

	// get and call main()
	spvm_result_t fnMain = spvm_state_get_result(state, "main");
	spvm_state_prepare(state, fnMain);

	if (state->derivative_used) {
		spvm_result_t fragCoord = spvm_state_get_builtin(state, SpvBuiltInFragCoord);
		if (fragCoord)
			spvm_state_set_frag_coord(state, fragCoord->members[0].value.f, fragCoord->members[1].value.f, fragCoord->members[2].value.f, fragCoord->members[3].value.f);
	}

	spvm_state_step_into(state);

	// split source into lines
	std::vector<std::string> srcLines = SplitString(src, "\n");

	bool hasStepped = true;
	while (state->code_current) {
		int curLine = state->current_line-1;
		if (hasStepped) {
#ifdef _WIN32
			system("cls"); // I know...
#else 
			system("clear"); // I know...
#endif

			int lnBegin = std::max(0, curLine - 5);
			int lnEnd = std::min((int)srcLines.size() - 1, curLine + 5);
			for (int i = lnBegin; i <= lnEnd; i++) {
				if (i == curLine)
					printf(">>>> | %s\n", srcLines[i].c_str());
				else
					printf("%4d | %s\n", i + 1, srcLines[i].c_str());
			}
			hasStepped = false;
		}

		if (is_emit_vertex_called) {
			printf(">>>>>>> EmitVertex <<<<<<<\n");
			PrintAllOutputValues(state);
			is_emit_vertex_called = false;
		}
		if (is_end_primitive_called) {
			printf(">>>>>>> EndPrimitive <<<<<<<\n");
			is_end_primitive_called = false;
		}

		printf("> ");
		std::string cmd;
		std::getline(std::cin, cmd);
		std::vector<std::string> tokens = SplitString(cmd, " ");

		if (tokens.size() == 0)
			continue;

		if (tokens[0] == "quit" || tokens[0] == "q")
			break;
		else if (tokens[0] == "step") {
			spvm_state_step_into(state);
			hasStepped = true;
		}
		else if (tokens[0] == "jump") {
			if (tokens.size() > 1) {
				int lnGoal = std::stoi(tokens[1]);
				while (state->code_current && state->current_line != lnGoal)
					spvm_state_step_into(state);
				hasStepped = true;
			}
		}
		else if (tokens[0] == "get") { // getlocal name
			if (tokens.size() > 1) {
				spvm_word mem_index = 0, mem_count = 0;
				spvm_result_t val = spvm_state_get_local_result(state, state->current_function, (spvm_string)tokens[1].c_str());
				if (val == nullptr)
					val = spvm_state_get_result(state, (spvm_string)tokens[1].c_str());
				
				if (val == nullptr) { // anon buffer object uniforms
					val = spvm_state_get_result(state, "");

					bool foundBufferVariable = false;
					for (spvm_word i = 0; i < prog->bound; i++) {
						if (state->results[i].name) {
							if (strcmp(state->results[i].name, "") == 0 && state->results[i].type == spvm_result_type_variable) {
								val = &state->results[i];
								spvm_result_t buffer_info = spvm_state_get_type_info(state->results, &state->results[val->pointer]);
								for (spvm_word i = 0; i < buffer_info->member_name_count; i++) {
									if (strcmp(tokens[1].c_str(), buffer_info->member_name[i]) == 0) {
										mem_index = i;
										mem_count = 1;

										foundBufferVariable = true;
										break;
									}
								}
							}
						}

						if (foundBufferVariable) break;
					}
				} else {
					mem_index = 0;
					mem_count = val->member_count;
				}

				if (val && mem_count != 0) {
					spvm_result_t type_info = spvm_state_get_type_info(state->results, &state->results[val->pointer]);
					OutputVariableValue(state, type_info, &val->members[mem_index], mem_count, "");
				}
				else printf("Variable \"%s\" doesn't exist.\n", tokens[1].c_str());
			}
		}
	}

	// outputs
	PrintAllOutputValues(state);

	// spirv-vm deinitialization
	spvm_state_delete(state);
	spvm_program_delete(prog);
	free(glsl_ext_data);
	spvm_context_deinitialize(ctx);

    return 0;
}


const TBuiltInResource DefaultTBuiltInResource = {
	/* .MaxLights = */ 32,
	/* .MaxClipPlanes = */ 6,
	/* .MaxTextureUnits = */ 32,
	/* .MaxTextureCoords = */ 32,
	/* .MaxVertexAttribs = */ 64,
	/* .MaxVertexUniformComponents = */ 4096,
	/* .MaxVaryingFloats = */ 64,
	/* .MaxVertexTextureImageUnits = */ 32,
	/* .MaxCombinedTextureImageUnits = */ 80,
	/* .MaxTextureImageUnits = */ 32,
	/* .MaxFragmentUniformComponents = */ 4096,
	/* .MaxDrawBuffers = */ 32,
	/* .MaxVertexUniformVectors = */ 128,
	/* .MaxVaryingVectors = */ 8,
	/* .MaxFragmentUniformVectors = */ 16,
	/* .MaxVertexOutputVectors = */ 16,
	/* .MaxFragmentInputVectors = */ 15,
	/* .MinProgramTexelOffset = */ -8,
	/* .MaxProgramTexelOffset = */ 7,
	/* .MaxClipDistances = */ 8,
	/* .MaxComputeWorkGroupCountX = */ 65535,
	/* .MaxComputeWorkGroupCountY = */ 65535,
	/* .MaxComputeWorkGroupCountZ = */ 65535,
	/* .MaxComputeWorkGroupSizeX = */ 1024,
	/* .MaxComputeWorkGroupSizeY = */ 1024,
	/* .MaxComputeWorkGroupSizeZ = */ 64,
	/* .MaxComputeUniformComponents = */ 1024,
	/* .MaxComputeTextureImageUnits = */ 16,
	/* .MaxComputeImageUniforms = */ 8,
	/* .MaxComputeAtomicCounters = */ 8,
	/* .MaxComputeAtomicCounterBuffers = */ 1,
	/* .MaxVaryingComponents = */ 60,
	/* .MaxVertexOutputComponents = */ 64,
	/* .MaxGeometryInputComponents = */ 64,
	/* .MaxGeometryOutputComponents = */ 128,
	/* .MaxFragmentInputComponents = */ 128,
	/* .MaxImageUnits = */ 8,
	/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/* .MaxCombinedShaderOutputResources = */ 8,
	/* .MaxImageSamples = */ 0,
	/* .MaxVertexImageUniforms = */ 0,
	/* .MaxTessControlImageUniforms = */ 0,
	/* .MaxTessEvaluationImageUniforms = */ 0,
	/* .MaxGeometryImageUniforms = */ 0,
	/* .MaxFragmentImageUniforms = */ 8,
	/* .MaxCombinedImageUniforms = */ 8,
	/* .MaxGeometryTextureImageUnits = */ 16,
	/* .MaxGeometryOutputVertices = */ 256,
	/* .MaxGeometryTotalOutputComponents = */ 1024,
	/* .MaxGeometryUniformComponents = */ 1024,
	/* .MaxGeometryVaryingComponents = */ 64,
	/* .MaxTessControlInputComponents = */ 128,
	/* .MaxTessControlOutputComponents = */ 128,
	/* .MaxTessControlTextureImageUnits = */ 16,
	/* .MaxTessControlUniformComponents = */ 1024,
	/* .MaxTessControlTotalOutputComponents = */ 4096,
	/* .MaxTessEvaluationInputComponents = */ 128,
	/* .MaxTessEvaluationOutputComponents = */ 128,
	/* .MaxTessEvaluationTextureImageUnits = */ 16,
	/* .MaxTessEvaluationUniformComponents = */ 1024,
	/* .MaxTessPatchComponents = */ 120,
	/* .MaxPatchVertices = */ 32,
	/* .MaxTessGenLevel = */ 64,
	/* .MaxViewports = */ 16,
	/* .MaxVertexAtomicCounters = */ 0,
	/* .MaxTessControlAtomicCounters = */ 0,
	/* .MaxTessEvaluationAtomicCounters = */ 0,
	/* .MaxGeometryAtomicCounters = */ 0,
	/* .MaxFragmentAtomicCounters = */ 8,
	/* .MaxCombinedAtomicCounters = */ 8,
	/* .MaxAtomicCounterBindings = */ 1,
	/* .MaxVertexAtomicCounterBuffers = */ 0,
	/* .MaxTessControlAtomicCounterBuffers = */ 0,
	/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
	/* .MaxGeometryAtomicCounterBuffers = */ 0,
	/* .MaxFragmentAtomicCounterBuffers = */ 1,
	/* .MaxCombinedAtomicCounterBuffers = */ 1,
	/* .MaxAtomicCounterBufferSize = */ 16384,
	/* .MaxTransformFeedbackBuffers = */ 4,
	/* .MaxTransformFeedbackInterleavedComponents = */ 64,
	/* .MaxCullDistances = */ 8,
	/* .MaxCombinedClipAndCullDistances = */ 8,
	/* .MaxSamples = */ 4,

	/* .maxMeshOutputVerticesNV = */ 256,
	/* .maxMeshOutputPrimitivesNV = */ 512,
	/* .maxMeshWorkGroupSizeX_NV = */ 32,
	/* .maxMeshWorkGroupSizeY_NV = */ 1,
	/* .maxMeshWorkGroupSizeZ_NV = */ 1,
	/* .maxTaskWorkGroupSizeX_NV = */ 32,
	/* .maxTaskWorkGroupSizeY_NV = */ 1,
	/* .maxTaskWorkGroupSizeZ_NV = */ 1,
	/* .maxMeshViewCountNV = */ 4,

	/* .limits = */ {
		/* .nonInductiveForLoops = */ 1,
		/* .whileLoops = */ 1,
		/* .doWhileLoops = */ 1,
		/* .generalUniformIndexing = */ 1,
		/* .generalAttributeMatrixVectorIndexing = */ 1,
		/* .generalVaryingIndexing = */ 1,
		/* .generalSamplerIndexing = */ 1,
		/* .generalVariableIndexing = */ 1,
		/* .generalConstantMatrixVectorIndexing = */ 1,
	}
};

bool Compile(std::vector<unsigned int>& outSPV, glslang::EShSource inLang, EShLanguage shaderType, const std::string& source, const std::string& entry)
{
	const char* inputStr = source.c_str();

	glslang::TShader shader(shaderType);
	if (entry.size() > 0 && entry != "main") {
		shader.setEntryPoint(entry.c_str());
		shader.setSourceEntryPoint(entry.c_str());
	}
	shader.setStrings(&inputStr, 1);

	// set up
	int sVersion = (shaderType == EShLanguage::EShLangCompute) ? 430 : 330;
	glslang::EShTargetClientVersion vulkanVersion = glslang::EShTargetClientVersion::EShTargetOpenGL_450;
	glslang::EShTargetLanguageVersion targetVersion = glslang::EShTargetLanguageVersion::EShTargetSpv_1_0;

	shader.setEnvInput(inLang, shaderType, glslang::EShClientOpenGL, sVersion);
	shader.setEnvClient(glslang::EShClientOpenGL, vulkanVersion);
	shader.setEnvTarget(glslang::EShTargetSpv, targetVersion);
	shader.setAutoMapLocations(true);


	TBuiltInResource res = DefaultTBuiltInResource;
	EShMessages messages = (EShMessages)(EShMsgSpvRules);

	const int defVersion = sVersion;

	std::string processedShader;

	DirStackFileIncluder includer;
	if (!shader.preprocess(&res, defVersion, ENoProfile, false, false, messages, &processedShader, includer)) {
		printf("%s\n", shader.getInfoLog());
		return false;
	}

	// update strings
	const char* processedStr = processedShader.c_str();
	shader.setStrings(&processedStr, 1);

	// parse
	if (!shader.parse(&res, 100, false, messages)) {
		printf("%s\n", shader.getInfoLog());
		return false;
	}

	// link
	glslang::TProgram prog;
	prog.addShader(&shader);

	if (!prog.link(messages)) {
		printf("%s\n", shader.getInfoLog());
		return false;
	}

	// convert to spirv
	spv::SpvBuildLogger logger;
	glslang::SpvOptions spvOptions;

	spvOptions.optimizeSize = false;
	spvOptions.disableOptimizer = true;
	spvOptions.generateDebugInfo = true;

	glslang::GlslangToSpv(*prog.getIntermediate(shaderType), outSPV, &logger, &spvOptions);

	return true;
}
