#include <stdio.h>
#include <string.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <cxxopts/include/cxxopts.hpp>

#include <ShaderDebugger/Utils.h>
#include <ShaderDebugger/Texture.h>
#include <ShaderDebugger/GLSLLibrary.h>
#include <ShaderDebugger/GLSLCompiler.h>
#include <ShaderDebugger/HLSLLibrary.h>
#include <ShaderDebugger/HLSLCompiler.h>
#include <ShaderDebugger/ShaderDebugger.h>

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

std::pair<int, uint8_t> GetVectorData(const std::string& vectype)
{
	static const std::vector<std::string> vecNames =
	{ 
		"float4", "int4", "bool4", "half4", "uint4",
		"float3", "int3", "bool3", "half3", "uint3",
		"float2", "int2", "bool2", "half2", "uint2",
		"vec4", "ivec4", "bvec4", "dvec4", "uvec4",
		"vec3", "ivec3", "bvec3", "dvec3", "uvec3"
		"vec2", "ivec2", "bvec2", "dvec2", "uvec2"
	};
	static const std::vector<bv_type> vecTypes =
	{ 
		bv_type_float, bv_type_int, bv_type_uchar, bv_type_float, bv_type_uint
	};
	static const std::vector<int> vecSizes =
	{
		4,3,2
	};

	for (int i = 0; i < vecNames.size(); i++)
		if (vecNames[i] == vectype)
			return std::make_pair(vecSizes[(i/5)%3], vecTypes[i % 5]);

	return std::make_pair<int, uint8_t>(0, bv_type_void);
}

void OutputVariableValue(bv_variable* val)
{
	if (val == nullptr)
		return;

	if (val->type == bv_type_float)
		printf("%.4f\n", sd::AsFloat(*val));
	else if (val->type == bv_type_int)
		printf("%d\n", sd::AsInteger(*val));
	else if (val->type == bv_type_uint)
		printf("%u\n", sd::AsUnsignedInteger(*val));
	else if (val->type == bv_type_uchar || val->type == bv_type_char)
		printf("%d\n", (int)sd::AsBool(*val));
	else if (val->type == bv_type_object) {
		bv_object* obj = bv_variable_get_object(*val);
		std::string objtypeName(obj->type->name);

		// vectors
		bool isVector = false;
		std::pair<int, bv_type> vecData = GetVectorData(objtypeName);
		if (vecData.second != bv_type_void) // vecX or gvecX
		{
			const char* fieldNames = "xyzw";

			u8 vecLen = obj->type->props.name_count;

			if (vecLen >= 2 && vecLen <= 4) {
				bv_type vecType = vecData.second;

				// actual vector
				if (vecType != bv_type_void) {
					if (vecType == bv_type_float) {
						glm::vec4 vecVal(0.0f);

						switch (vecLen) {
						case 2: vecVal = glm::vec4(sd::AsVector<2, float>(*val), 0.0f, 0.0f); break;
						case 3: vecVal = glm::vec4(sd::AsVector<3, float>(*val), 0.0f); break;
						case 4: vecVal = sd::AsVector<4, float>(*val); break;
						}

						for (u8 i = 0; i < vecLen; i++)
							printf("%c=%.2f ", fieldNames[i], vecVal[i]);
						printf("\n");
					}
					else if (vecType == bv_type_int) {
						glm::ivec4 vecVal(0);

						switch (vecLen) {
						case 2: vecVal = glm::ivec4(sd::AsVector<2, int>(*val), 0, 0); break;
						case 3: vecVal = glm::ivec4(sd::AsVector<3, int>(*val), 0); break;
						case 4: vecVal = sd::AsVector<4, int>(*val); break;
						}

						for (u8 i = 0; i < vecLen; i++)
							printf("%c=%d ", fieldNames[i], vecVal[i]);
						printf("\n");
					}
					else if (vecType == bv_type_uint) {
						glm::uvec4 vecVal(0u);

						switch (vecLen) {
						case 2: vecVal = glm::uvec4(sd::AsVector<2, unsigned int>(*val), 0u, 0u); break;
						case 3: vecVal = glm::uvec4(sd::AsVector<3, unsigned int>(*val), 0u); break;
						case 4: vecVal = sd::AsVector<4, unsigned int>(*val); break;
						}

						for (u8 i = 0; i < vecLen; i++)
							printf("%c=%u ", fieldNames[i], vecVal[i]);
						printf("\n");
					}
					else if (vecType == bv_type_uchar) {
						glm::bvec4 vecVal(false);

						switch (vecLen) {
						case 2: vecVal = glm::bvec4(sd::AsVector<2, bool>(*val), false, false); break;
						case 3: vecVal = glm::bvec4(sd::AsVector<3, bool>(*val), false); break;
						case 4: vecVal = sd::AsVector<4, bool>(*val); break;
						}

						for (u8 i = 0; i < vecLen; i++)
							printf("%c=%d ", fieldNames[i], (int)vecVal[i]);
						printf("\n");
					}
					isVector = true;
				}
			}
		}

		if (!isVector)
		{
			// matrices
			if (sd::GetMatrixTypeFromName(objtypeName.c_str()) != bv_type_void)
			{
				sd::Matrix mat = sd::AsMatrix(*val);
				for (int x = 0; x < mat.Columns; x++) {
					for (int y = 0; y < mat.Rows; y++)
						printf("%7.2f ", mat.Data[x][y]);
					printf("\n");
				}
			}
			// textures
			else if (sd::IsBasicTexture(objtypeName.c_str()))
			{
				sd::Texture* tex = (sd::Texture*)obj->user_data;
				printf("Texture %dx%dx%d\n", objtypeName, tex->Width, tex->Height, tex->Depth);
			}
			// others
			else
			{
				bool isNull = true;
				for (int i = 0; i < obj->type->props.name_count; i++) {
					if (obj->prop[i].type == bv_type_void)
						continue;
					isNull = false;
					printf(".%s = ", obj->type->props.names[i]);
					OutputVariableValue(&obj->prop[i]);
				}

				if (isNull)
					printf("null\n");
			}
		}
	}
	else if (val->type == bv_type_pointer)
		OutputVariableValue(bv_variable_get_pointer(*val));
	else if (val->type == bv_type_array) {
		bv_array* arr = bv_variable_get_array(*val);
		u16 index = 0;

		// 1D arrays only
		for (; index < arr->length[0]; index++) {
			bv_variable arrEl = bv_array_get(*arr, &index);
			OutputVariableValue(&arrEl);
		}
	}
}

int main(int argc, char* argv[]) {
	std::string filename = "", entry = "main";
	bool isHLSL = false;
	sd::ShaderType stage = sd::ShaderType::Pixel;

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
			isHLSL = (cmp == "hlsl");
		}
		if (optsResult.count("entry"))
			entry = optsResult["entry"].as<std::string>();
		if (optsResult.count("stage"))
			stage = (optsResult["stage"].as<std::string>() == "vertex" ? sd::ShaderType::Vertex : sd::ShaderType::Pixel);
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

	std::ifstream t(filename);
	std::string src((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
	t.close();

	sd::ShaderDebugger dbg;
	bool res = false;
	if (!isHLSL)
		res = dbg.SetSource<sd::GLSLCompiler>(stage, src, entry, NULL, sd::GLSL::Library());
	else
		res = dbg.SetSource<sd::HLSLCompiler>(stage, src, entry, NULL, sd::HLSL::Library());

	if (!res) {
		printf("[ERROR] Failed to compile the shader.\n");
		return 1;
	}

	// textures
	std::vector<sd::Texture*> texs;

	// initialize globals
	const auto& globals = dbg.GetCompiler()->GetGlobals();
	for (const auto& global : globals)
	{
		if (global.Storage == sd::Variable::StorageType::Uniform ||
			global.Storage == sd::Variable::StorageType::In)
		{
			// skip samplerstate for now
			if (global.Type == "SamplerState")
				continue;
			
			if (sd::IsBasicTexture(global.Type.c_str())) {
				printf("Please enter a path to the texture file for %s: ", global.Name.c_str());
				char filepath[1024];
				scanf("%s", filepath);

				int width, height, nrChannels;
				unsigned char* data = stbi_load(filepath, &width, &height, &nrChannels, 0);
				sd::Texture* tex = new sd::Texture();

				if (data == nullptr) {
					tex->Allocate(1, 1);
					tex->Fill(glm::vec4(1.0f));
					texs.push_back(tex);
					dbg.SetGlobalValue(global.Name, global.Type, tex);
					printf("Failed to load texture %s! :(\n", filepath);
					continue;
				}

				tex->Allocate(width, height);
				for (int x = 0; x < width; x++) {
					for (int y = 0; y < height; y++) {

						float* mem = &tex->Data[0][(x + tex->Width * (y + tex->Depth * 0)) * 4];

						mem[3] = 1.0f;
						if (nrChannels >= 3) {
							mem[0] = data[(y * width + x) * 3 + 0] / 255.0f;
							mem[1] = data[(y * width + x) * 3 + 1] / 255.0f;
							mem[2] = data[(y * width + x) * 3 + 2] / 255.0f;
							if (nrChannels >= 4)
								mem[3] = data[(y * width + x) * 3 + 3] / 255.0f;
						}
						else if (nrChannels == 1) {
							float val = data[(y * width + x) * 1 + 0] / 255.0f;
							mem[0] = val;
							mem[1] = val;
							mem[2] = val;
						}
					}
				}
				texs.push_back(tex);

				stbi_image_free(data);

			} else {
				printf("Please enter value for %s %s: ", global.Type.c_str(), global.Name.c_str());

				if (global.Type == "float") {
					float val;
					scanf(" %f", &val);
					dbg.SetGlobalValue(global.Name, val);
				}
				// vectors
				else {
					int cols = 0, rows = 0;
					sd::GetMatrixSizeFromName(global.Type.c_str(), &cols, &rows);

					if (cols == 0 && rows == 0) {
						glm::vec4 inData(0.0f);

						// hacky way to get # of components
						u16 propCount = 2;
						for (int i = 0; i < global.Type.size(); i++)
							if (isdigit(global.Type[i]))
								propCount = global.Type[i] - '0';

						for (int i = 0; i < propCount; i++)
							scanf(" %f", &inData[i]);

						dbg.SetGlobalValue(global.Name, global.Type, inData);
					}
					else printf("TODO: matrix input\n");
				}
			}
		}
	}

	// initialize arguments
	const auto& funcs = dbg.GetCompiler()->GetFunctions();
	for (const auto& func : funcs) {
		if (func.Name == entry) // TODO: == EntryFunction
		{
			const auto& args = func.Arguments;
			if (args.size() != 0) {
				bv_stack argStack = bv_stack_create();
				for (const auto& arg : args) {
					if (!arg.Semantic.empty()) {
						bv_variable semValue = dbg.GetSemanticValue(arg.Semantic);

						if (semValue.type != bv_type_void) {
							bv_stack_push(&argStack, semValue);
							continue;
						}
					}

					printf("Please enter value for %s %s: ", arg.Type.c_str(), arg.Name.c_str());

					if (arg.Type == "float") {
						float val;
						scanf(" %f", &val);
						bv_stack_push(&argStack, bv_variable_create_float(val));
					}
					else printf("TODO: other types for main() arguments\n");
					// TODO: others + merge this with uniform input thingy
				}
				dbg.SetArguments(&argStack);
			}
		}
	}

	// gl_fragcoord
	if (!isHLSL) {
		float fragX, fragY;
		dbg.AddGlobal("gl_FragCoord");
		printf("gl_FragCoord: ");
		scanf("%f %f", &fragX, &fragY);
		dbg.SetGlobalValue("gl_FragCoord", "vec4", glm::vec4(fragX, fragY, 0.0f, 0.0f));
	}

	// skip initialization
	dbg.Step();

	std::vector<std::string> srcLines = SplitString(src, "\n");

	// debugger
	bool hasStepped = true;
	while (true) {
		int curLine = dbg.GetCurrentLine()-1;
		if (hasStepped) {
			#ifdef _WIN32
			system("cls"); // I know...
			#else 
			system("clear"); // I know...
			#endif

			int lnBegin = std::max(0, curLine-5);
			int lnEnd = std::min((int)srcLines.size()-1, curLine+5);
			for (int i = lnBegin; i <= lnEnd; i++) {
				char hasBreakpoint = dbg.HasBreakpoint(i+1) ? 'x' : ' ';
				if (i == curLine)
					printf(">>>>%c| %s\n", hasBreakpoint, srcLines[i].c_str());
				else
					printf("%4d%c| %s\n", i+1, hasBreakpoint, srcLines[i].c_str());
			}
			hasStepped = false;
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
			bool ret = dbg.Step();
			if (!ret) break;
			hasStepped = true;
		}
		else if (tokens[0] == "stepover") {
			bool ret = dbg.StepOver();
			if (!ret) break;
			hasStepped = true;
		}
		else if (tokens[0] == "stepout") {
			bool ret = dbg.StepOut();
			if (!ret) break;
			hasStepped = true;
		}
		else if (tokens[0] == "jump") {
			if (tokens.size() > 1) {
				int lnGoal = std::stoi(tokens[1]);
				dbg.Jump(lnGoal);
				hasStepped = true;
			}
		}
		else if (tokens[0] == "get") { // getlocal name
			if (tokens.size() > 1) {
				bv_variable* val = dbg.GetLocalValue(tokens[1]);
				if (val == nullptr)
					val = dbg.GetGlobalValue(tokens[1]);

				OutputVariableValue(val);
			}
		}
		else if (tokens[0] == "imd") { // immediate
			if (tokens.size() > 1) {
				bv_variable val = dbg.Immediate(cmd.substr(4));
				OutputVariableValue(&val);
				bv_variable_deinitialize(&val);
			}
		}
		else if (tokens[0] == "continue") {
			bool res = dbg.Continue();
			if (!res) break;
			hasStepped = true; // update the view
		}
		else if (tokens[0] == "bkpt") {
			if (tokens.size() > 1) {
				int line = std::stoi(tokens[1]);
				dbg.AddBreakpoint(line);
				hasStepped = true; // update the view
			}
		}
		else if (tokens[0] == "cbkpt") {
			if (tokens.size() > 1) {
				int line = std::stoi(tokens[1]);
				
				size_t pos = 0;
				for (size_t i = 0; i < cmd.size(); i++)
					if (isdigit(cmd[i])) {
						pos = i;
						break;
					}
				pos = cmd.find_first_of(' ', pos);

				dbg.AddConditionalBreakpoint(line, cmd.substr(pos));

				hasStepped = true; // update the view
			}
		}
		else if (tokens[0] == "rembkpt") {
			if (tokens.size() > 1) {
				int line = std::stoi(tokens[1]);
				dbg.ClearBreakpoint(line);
			}
			else
				dbg.ClearBreakpoints();
			hasStepped = true;
		}
		else if (tokens[0] == "func")
			printf("%s\n", dbg.GetCurrentFunction().c_str());
		else if (tokens[0] == "fstack") {
			std::vector<std::string> fstack = dbg.GetFunctionStack();
			for (int i = 0; i < fstack.size(); i++) {
				printf("%s", fstack[i].c_str());
				if (i != fstack.size()-1)
					printf(" -> ");
			}
			printf("\n");
		}
	}

	printf("Finished debugging!\n");
	printf("Discarded: %d\n", (int)dbg.IsDiscarded());

	// if (isGLSL) {
		printf("Output:\n");
		for (const auto& gl : globals) {
			if (gl.Storage == sd::Variable::StorageType::Out) {
				printf("\t%s = ", gl.Name.c_str());
				OutputVariableValue(dbg.GetGlobalValue(gl.Name));
			}
		}
	// }

	// if (isHLSL) {
		bv_variable retValue = dbg.GetReturnValue();
		if (retValue.type != bv_type_void) {
			printf("Return: ");
			OutputVariableValue(&retValue);
		}
		bv_variable_deinitialize(&retValue);
	// }

	for (int i = 0; i < texs.size(); i++)
		delete texs[i];

    return 0;
}