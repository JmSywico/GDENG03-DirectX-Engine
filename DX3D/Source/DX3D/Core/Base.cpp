#include <DX3D/Core/Base.h>


dx3d::Base::Base(const BaseDesc& desc): m_logger(desc.logger)
{
}

dx3d::Base::~Base()
{
}

dx3d::Logger& dx3d::Base::getLogger() noexcept
{
	return m_logger;
}
