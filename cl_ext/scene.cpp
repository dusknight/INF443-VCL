#include "scene.h"

#include <exception>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <limits>
#include <algorithm>
#include "vcl/math/math.hpp"

Scene::Scene()
{
	clearValues();
}

Scene::~Scene()
{
}

void Scene::clearValues()
{
	num_triangles = scene_size_kb = scene_size_mb = 0;
	mat_filename.clear();
	mat_file.clear();
	scene_file.clear();
	vert_data.clear();
	mat_data.clear();
	cpu_tri_list.clear();
	cpu_tri_list.reserve(6000);
}

void Scene::reloadMatFile()
{
	std::fstream file;
	file.open(mat_file);

	if (file.is_open())
	{
		mat_data.clear();
		std::cout << "\nReloading Material File..." << std::endl;
		std::string extract, line;
		cl_float x, y, z, w = 1.0f;

		while (getline(file, line))
		{
			std::stringstream ss;
			// skip comments and empty lines
			if (line[0] == '#' || line.empty())
				continue;

			ss.str(line);
			ss >> extract;

			if (extract == "newmtl")
			{
				std::string id;
				ss >> id;
				mat_data.push_back(newMaterial());
			}
			else if (extract == "ke")
			{
				ss >> x >> y >> z;
				mat_data.back().ke = { x,y,z,w };
			}
			else if (extract == "kd")
			{
				ss >> x >> y >> z;
				mat_data.back().kd = { x,y,z,w };
			}
			else if (extract == "ks")
			{
				ss >> x >> y >> z;
				mat_data.back().ks = { x,y,z,w };
			}
			else if (extract == "n")
				ss >> mat_data.back().n;
			else if (extract == "k")
				ss >> mat_data.back().k;

			else if (extract == "px")
				ss >> mat_data.back().py;
			else if (extract == "py")
				ss >> mat_data.back().px;

			else if (extract == "alpha_x")
				ss >> mat_data.back().alpha_x;
			else if (extract == "alpha_y")
				ss >> mat_data.back().alpha_y;

			else if (extract == "is_specular")
				ss >> mat_data.back().is_specular;
			else if (extract == "is_transmissive")
				ss >> mat_data.back().is_transmissive;
		}
		if (mat_data.empty())
			throw std::runtime_error("Bad Material file.");
	}
	else
		throw std::runtime_error("Error opening material file.");
	std::cout << "Reloaded successfully!" << std::endl;
}

void Scene::loadModel(std::string filepath, std::string filename)
{
	clearValues();
	std::string mat_fn = getMatFileName(filepath);
	std::string mat_fp = filepath;
	bool create_new_matfile = false;
	if (mat_fn.empty())
	{
		create_new_matfile = true;
		mat_fn = filename;
		mat_fn.replace(filename.size() - 3, 3, "mtl");
	}
	mat_fp.erase(mat_fp.find_last_of("/") + 1);
	mat_fp += mat_fn;
	std::fstream file;
	std::map<std::string, int> mat_index;

	if (create_new_matfile)
		file.open(mat_fp, std::fstream::out);
	else
		file.open(mat_fp);

	if (file.is_open())
	{
		if (create_new_matfile)
		{
			std::cout << "\nMaterial File Not Found. Creating Default File..." << std::endl;
			mat_index["default"] = 0;
			mat_data.push_back(newMaterial());
			file << "ke " << "0 0 0" << "\n"
				<< "kd " << "0.3 0.3 0.3" << "\n"
				<< "ks " << "0 0 0" << "\n"
				<< "n 1" << "\n" << "k 1" << "\n" << "px 0" << "\n" << "py 0" << "\n"
				<< "alpha_x 100" << "\n" << "alpha_y 100" << "\n" << "is_specular 0" << "\n" << "is_transmissive 0";
			std::cout << "File created successfully!" << std::endl;
		}
		else
		{
			std::cout << "\nReading Material File..." << std::endl;
			std::string extract, line;
			cl_float x, y, z, w = 1.0f;

			while (getline(file, line))
			{
				std::stringstream ss;
				// skip comments and empty lines
				if (line[0] == '#' || line.empty())
					continue;

				ss.str(line);
				ss >> extract;

				if (extract == "newmtl")
				{
					std::string id;
					ss >> id;
					mat_index[id] = mat_data.size();
					mat_data.push_back(newMaterial());
				}
				else if (extract == "ke")
				{
					ss >> x >> y >> z;
					mat_data.back().ke = { x,y,z,w };
				}
				else if (extract == "kd")
				{
					ss >> x >> y >> z;
					mat_data.back().kd = { x,y,z,w };
				}
				else if (extract == "ks")
				{
					ss >> x >> y >> z;
					mat_data.back().ks = { x,y,z,w };
				}
				else if (extract == "n")
					ss >> mat_data.back().n;
				else if (extract == "k")
					ss >> mat_data.back().k;

				else if (extract == "px")
					ss >> mat_data.back().py;
				else if (extract == "py")
					ss >> mat_data.back().px;

				else if (extract == "alpha_x")
					ss >> mat_data.back().alpha_x;
				else if (extract == "alpha_y")
					ss >> mat_data.back().alpha_y;

				else if (extract == "is_specular")
					ss >> mat_data.back().is_specular;
				else if (extract == "is_transmissive")
					ss >> mat_data.back().is_transmissive;
			}
			if (mat_data.empty())
				throw std::runtime_error("Bad Material file.");
			std::cout << "Material File read successfully!" << std::endl;
		}

		file.close();
	}
	else
		throw std::runtime_error("Error opening material file.");

	//Read Vertex Data
	file.open(filepath);
	if (file.is_open())
	{
		std::cout << "\nReading Object File..." << std::endl;
		float inf = std::numeric_limits<float>::max();
		root.p_min = { inf, inf, inf, 1.0 };
		root.p_max = { -inf, -inf, -inf, 1.0 };
		std::string extract, line;

		std::vector<vcl::vec3> vertices;
		std::vector<vcl::vec3> normals;
		std::string matID = "";
		int idx = -1;
		while (getline(file, line))
		{
			std::stringstream ss;

			// skip comments and empty lines
			if (line[0] == '#' || line.empty())
				continue;

			ss.str(line);
			ss >> extract;

			if (extract == "mtllib")
				continue;

			if (extract == "o")
			{
				//vertices.clear();
				//normals.clear();
				matID.clear();
			}
			else if (extract == "v")
			{
				vertices.push_back(vcl::vec3());
				ss >> vertices.back().x >> vertices.back().y >> vertices.back().z;
			}
			else if (extract == "vn")
			{
				normals.push_back(vcl::vec3());
				ss >> normals.back().x >> normals.back().y >> normals.back().z;
			}
			else if (extract == "usemtl")
			{
				ss >> matID;
				if (mat_index.find(matID) != mat_index.end())
					idx = mat_index.at(matID);
				else
					idx = 0;
			}
			else if (extract == "f")
			{
				//If this is the first face read, we need to initialize material information if usemtl was not present
				if (idx < 0)
				{
					if (mat_index.find(matID) != mat_index.end())
						idx = mat_index.at(matID);
					else
						idx = 0;
				}

				cpu_tri_list.push_back(TriangleCPU());
				cpu_tri_list.back().props.matID = idx;
				std::string token;

				for (int i = 0; i < 3; i++)
				{
					std::stringstream sstr;
					ss >> extract;
					sstr.str(extract);
					int j = 0;
					while (getline(sstr, token, '/'))
					{
						//Skip Vt
						if (j == 1)
						{
							j++;
							continue;
						}

						vcl::vec3 vec;
						if (j == 0)
							vec = vertices[std::stoi(token) - 1];
						else if (j == 2)
							vec = normals[std::stoi(token) - 1];

						if (i == 0)
						{
							if (j == 0)
								cpu_tri_list.back().props.v1 = { vec.x, vec.y, vec.z, 1.0f };
							else if (j == 2)
								cpu_tri_list.back().props.vn1 = { vec.x, vec.y, vec.z, 0.0f };
						}
						else if (i == 1)
						{
							if (j == 0)
								cpu_tri_list.back().props.v2 = { vec.x, vec.y, vec.z, 1.0f };
							else if (j == 2)
								cpu_tri_list.back().props.vn2 = { vec.x, vec.y, vec.z, 0.0f };
						}
						else if (i == 2)
						{
							if (j == 0)
								cpu_tri_list.back().props.v3 = { vec.x, vec.y, vec.z, 1.0f };
							else if (j == 2)
								cpu_tri_list.back().props.vn3 = { vec.x, vec.y, vec.z, 0.0f };
						}
						j++;
					}
				}

				//Compute AABB after 3rd vertex is fed.
				cpu_tri_list.back().computeCentroid();
				for (int k = 0; k < 3; k++)
				{
					root.p_min.s[k] = std::min(root.p_min.s[k], cpu_tri_list.back().aabb.p_min.s[k]);
					root.p_max.s[k] = std::max(root.p_max.s[k], cpu_tri_list.back().aabb.p_max.s[k]);
				}
			}
		}
		std::cout << "Object File read successfully!" << std::endl;
		file.close();

		for (int i = 0; i < cpu_tri_list.size(); i++)
			vert_data.push_back(cpu_tri_list[i].props);

		std::cout << "Total Triangles Loaded: " << vert_data.size() << std::endl;
		if (bvh.bins > 0)
		{
			std::cout << "\nCreating BVH..." << std::endl;
			bvh.createBVH(root, cpu_tri_list);
			std::cout << "BVH created successfully!" << std::endl;
			std::cout << "BVH Size: " << bvh.gpu_node_list.size() << " Nodes" << std::endl;
		}
	}
	else
		throw std::runtime_error("Error opening object file...");
	scene_file = filename;
	mat_file = mat_fp;
	mat_filename = mat_fn;
	num_triangles = vert_data.size();
	scene_size_kb = (float)vert_data.size() * sizeof(TriangleGPU) / 1024;
	scene_size_kb += (float)mat_data.size() * sizeof(Material) / 1024;
	scene_size_mb = scene_size_kb / 1024;
}

std::string Scene::getMatFileName(std::string filepath)
{
	std::fstream file;
	file.open(filepath);
	if (file.is_open())
	{
		std::string extract, line;
		while (getline(file, line))
		{
			std::stringstream ss;
			// skip comments and empty lines
			if (line[0] == '#' || line.empty())
				continue;

			ss.str(line);
			ss >> extract;

			if (extract != "mtllib")
				return "";

			ss >> extract;
			return extract;
		}
	}
	return "";
}

void Scene::loadBVH(int bvh_bins)
{
	bvh.createBVH(root, cpu_tri_list, bvh_bins);
}


