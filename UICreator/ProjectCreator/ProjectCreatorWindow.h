#pragma once
#include "ProjectCreator.h"
#include <functional>
class PropertyWindow;
namespace ProjectCreatorWindow {
void Open(PropertyWindow* properties,std::function<ProjectCreation::json()> project,const std::string& sourceDirectory,const std::string& templateId={});
void Close();
}
