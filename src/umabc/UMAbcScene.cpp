/**
 * @file UMAbcScene.cpp
 * any
 *
 * @author tori31001 at gmail.com
 *
 * Copyright (C) 2013 Kazuma Hatta
 * Licensed  under the MIT license. 
 *
 */
#include "UMAbcScene.h"

#include <memory>
#include <algorithm>
#include <Alembic/Abc/All.h>
#include <Alembic/AbcGeom/All.h>
#include <Alembic/AbcCoreFactory/All.h>

#include "UMAbcObject.h"
#include "UMAbcSoftwareIO.h"
#include "UMAbcMesh.h"
#include "UMAbcPoint.h"
#include "UMAbcCurve.h"
#include "UMAbcNurbsPatch.h"
#include "UMAbcCamera.h"
#include "UMAbcXform.h"

namespace umabc
{
	using namespace Alembic::Abc;
	using namespace Alembic::AbcGeom;

class UMAbcScene::SceneImpl
{
	DISALLOW_COPY_AND_ASSIGN(SceneImpl);
public:

	SceneImpl(UMAbcObjectPtr root)
		: object_(root)
		, pre_time_(-1)
		{}
	~SceneImpl() {}

	/**
	 * initialize
	 */
	bool init()
	{
		unsigned long current = object_->current_time_ms();
		if (object_->init(true, UMAbcObjectPtr()))
		{
			unsigned long min_time_ = object_->min_time();
			unsigned long max_time_ = object_->max_time();

			// set initial time
			if (min_time_ <= max_time_)
			{
				object_->set_current_time(min_time_, true);
			}
			else
			{
				object_->set_current_time(0, true);
			}

			// calculate bounding box
			object_->update_box(true);
			object_->set_current_time(current, true);
		}
		return true;
	}
	
	bool update(unsigned long time)
	{
		if (is_constant()) return true;
		if (pre_time_ == time) return true;
		if (time < min_time() || time > max_time())
		{
			return false;
		}
		if (object_)
		{
			object_->set_current_time(time, true);
		}
		pre_time_ = time;
		return true;
	}

	bool clear() 
	{
		return true;
	}

	/**
	 * get minimum time
	 */
	double min_time() const
	{
		if (object_)
		{
			return object_->min_time();
		}
		return 0.0;
	}
	
	/**
	 * get maximum time
	 */
	double max_time() const
	{
		if (object_)
		{
			return object_->max_time();
		}
		return 0.0;
	}

	/**
	 * get name list
	 */
	void object_name_list(std::vector<std::string>& name_list)
	{
		if (object_)
		{
			name_list.push_back(object_->name());
			get_name_list_recursive(name_list, object_);
		}
	}

	/**
	* get name list
	*/
	template <class T>
	void path_list(std::vector<std::string>& name_list)
	{
		if (object_)
		{
			//name_list.push_back(object_->name());
			for (UMAbcObjectList::const_iterator it = object_->children().begin();
				it != object_->children().end();
				++it)
			{
				std::string object_path = "/" + (*it)->name();
				if (std::dynamic_pointer_cast<T>(*it)) {
					name_list.push_back(object_path);
				}
				get_path_list_recursive<T>(object_path, name_list, *it);
			}
		}
	}

	UMAbcObjectPtr find_object(const std::string& target_path)
	{
		if (object_)
		{
			for (UMAbcObjectList::const_iterator it = object_->children().begin();
				it != object_->children().end();
				++it)
			{
				std::string object_path = "/" + (*it)->name();
				if (UMAbcObjectPtr obj = find_object_recursive(object_path, target_path, *it)) {
					return obj;
				}
			}
		}
		return UMAbcObjectPtr();
	}

	UMAbcObjectPtr root_object() const { return object_; }
	
	/**
	 * get total polygons
	 */
	size_t total_polygon_size() const
	{
		size_t total_size = 0;
		total_polygon_size_recursive(total_size, root_object());
		return total_size;
	}

private:
	UMAbcObjectPtr object_;
	unsigned long pre_time_;

	/**
	 * is constant
	 */
	bool is_constant() const { return min_time() >= max_time(); }

	void get_name_list_recursive(std::vector<std::string>& name_list, UMAbcObjectPtr object)
	{
		for (UMAbcObjectList::const_iterator it = object_->children().begin();
			it != object_->children().end();
			++it)
		{
			name_list.push_back((*it)->name());
		}
	}

	template<class T>
	void get_path_list_recursive(std::string& object_path, std::vector<std::string>& name_list, UMAbcObjectPtr object)
	{
		for (UMAbcObjectList::const_iterator it = object->children().begin();
			it != object->children().end();
			++it)
		{
			std::string path = object_path + "/" + (*it)->name();
			if (std::dynamic_pointer_cast<T>(*it)) {
				name_list.push_back(path);
			}
			get_path_list_recursive<T>(path, name_list, *it);
		}
	}

	UMAbcObjectPtr find_object_recursive(std::string& object_path, const std::string& target, UMAbcObjectPtr object)
	{
		if (object_path == target) {
			return object;
		}
		for (UMAbcObjectList::const_iterator it = object->children().begin();
			it != object->children().end();
			++it)
		{
			std::string path = object_path + "/" + (*it)->name();
			if (UMAbcObjectPtr obj = find_object_recursive(path, target, *it)) {
				return obj;
			}
		}
		return UMAbcObjectPtr();
	}

	void total_polygon_size_recursive(size_t& dst_size, UMAbcObjectPtr object) const
	{
		if (!object) return;
		if (UMAbcMeshPtr mesh = std::dynamic_pointer_cast<UMAbcMesh>(object))
		{
			dst_size += mesh->triangle_index().size();
		}
		for (UMAbcObjectList::const_iterator it = object->children().begin();
			it != object->children().end();
			++it)
		{
			total_polygon_size_recursive(dst_size, *it);
		}
	}

	umabc::UMAbcCameraPtr find_camera_recursive(const std::string& name, UMAbcObjectPtr object)
	{
		if (!object) {
			return umabc::UMAbcCameraPtr();
		}
		if (UMAbcCameraPtr camera = std::dynamic_pointer_cast<UMAbcCamera>(object))
		{
			return camera;
		}
		for (UMAbcObjectList::const_iterator it = object->children().begin();
			it != object->children().end();
			++it)
		{
			if (umabc::UMAbcCameraPtr camera = find_camera_recursive(name, *it))
			{
				return camera;
			}
		}
		return umabc::UMAbcCameraPtr();
	}
};

/**
 * constructor
 */
UMAbcScene::UMAbcScene(UMAbcObjectPtr root)
	: impl_(new UMAbcScene::SceneImpl(root))
{
}

/**
 * destructor
 */
UMAbcScene::~UMAbcScene()
{
}

/**
 * get minimum time
 */
double UMAbcScene::min_time() const
{
	return impl_->min_time();
}
	
/**
 * get maximum time
 */
double UMAbcScene::max_time() const
{
	return impl_->max_time();
}

/**
 * initialize
 */
bool UMAbcScene::init()
{
	return impl_->init();
}

/**
 * release all resources. call this function before delete.
 */
bool UMAbcScene::dispose()
{
	impl_ = SceneImplPtr();
	return true;
}

/** 
 * update scene
 */
bool UMAbcScene::update(unsigned long time)
{
	return impl_->update(time);
}

/** 
 * clear scene
 */
bool UMAbcScene::clear()
{
	return impl_->clear();
}

/**
 * get name list
 */
std::vector<std::string> UMAbcScene::object_name_list()
{
	std::vector<std::string> name_list;
	impl_->object_name_list(name_list);
	return name_list;
}

/**
 * get path list
 */
std::vector<std::string> UMAbcScene::mesh_path_list()
{
	std::vector<std::string> name_list;
	impl_->path_list<UMAbcMesh>(name_list);
	return name_list;
}

/**
 * get path list
 */
std::vector<std::string> UMAbcScene::point_path_list()
{
	std::vector<std::string> name_list;
	impl_->path_list<UMAbcPoint>(name_list);
	return name_list;
}

/**
 * get path list
 */
std::vector<std::string> UMAbcScene::curve_path_list()
{
	std::vector<std::string> name_list;
	impl_->path_list<UMAbcCurve>(name_list);
	return name_list;
}

/**
 * get path list
 */
std::vector<std::string> UMAbcScene::nurbs_path_list()
{
	std::vector<std::string> name_list;
	impl_->path_list<UMAbcNurbsPatch>(name_list);
	return name_list;
}

/**
 * get path list
 */
std::vector<std::string> UMAbcScene::camera_path_list()
{
	std::vector<std::string> name_list;
	impl_->path_list<UMAbcCamera>(name_list);
	return name_list;
}

/**
 * get path list
 */
std::vector<std::string> UMAbcScene::xform_path_list()
{
	std::vector<std::string> name_list;
	impl_->path_list<UMAbcXform>(name_list);
	return name_list;
}

/**
 * get root object
 */
UMAbcObjectPtr UMAbcScene::root_object()
{
	return impl_->root_object();
}

/**
 * get total polygons
 */
size_t UMAbcScene::total_polygon_size() const
{
	return impl_->total_polygon_size();
}

/**
 * find object
 */
UMAbcObjectPtr UMAbcScene::find_object(const std::string& object_path)
{
	return impl_->find_object(object_path);
}

} // umabc
