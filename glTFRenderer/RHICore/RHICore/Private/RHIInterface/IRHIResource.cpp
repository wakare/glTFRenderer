#include "IRHIResource.h"

const std::string& IRHIResource::GetName() const
{
	return m_resource_name;
}

void IRHIResource::SetName(const std::string& name)
{
	m_resource_name = name;
}

void IRHIResource::SetNeedRelease()
{
	need_release = true;
}

bool IRHIResource::IsNeedRelease() const
{
	return need_release;
}
