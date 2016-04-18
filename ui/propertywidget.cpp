/*
  propertywidget.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2010-2016 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "propertywidget.h"

#include "common/endpoint.h"
#include "common/objectbroker.h"
#include "common/propertycontrollerinterface.h"

#include <algorithm>

using namespace GammaRay;

QVector<PropertyWidgetTabFactoryBase*> PropertyWidget::s_tabFactories = QVector<PropertyWidgetTabFactoryBase*>();
QVector<PropertyWidget*> PropertyWidget::s_propertyWidgets;

PropertyWidget::PropertyWidget(QWidget *parent)
  : QTabWidget(parent),
    m_controller(0)
{
  s_propertyWidgets.push_back(this);
}

PropertyWidget::~PropertyWidget()
{
  const int index = s_propertyWidgets.indexOf(this);
  if (index >= 0)
    s_propertyWidgets.remove(index);
}

QString PropertyWidget::objectBaseName() const
{
  Q_ASSERT(!m_objectBaseName.isEmpty());
  return m_objectBaseName;
}

void PropertyWidget::setObjectBaseName(const QString &baseName)
{
  Q_ASSERT(m_objectBaseName.isEmpty()); // ideally the object base name would be a ctor argument, but then this doesn't work in Designer anymore
  m_objectBaseName = baseName;

  if (Endpoint::instance()->objectAddress(baseName + ".controller") == Protocol::InvalidObjectAddress)
    return; // unknown property controller, likely disabled/not supported on the server

  if (m_controller) {
    disconnect(m_controller, SIGNAL(availableExtensionsChanged()), this, SLOT(updateShownTabs()));
  }
  m_controller = ObjectBroker::object<PropertyControllerInterface*>(m_objectBaseName + ".controller");
  connect(m_controller, SIGNAL(availableExtensionsChanged()), this, SLOT(updateShownTabs()));

  updateShownTabs();
}

void PropertyWidget::registerTab(PropertyWidgetTabFactoryBase* factory)
{
    s_tabFactories.push_back(factory);
    foreach (PropertyWidget *widget, s_propertyWidgets)
        widget->updateShownTabs();
}

void PropertyWidget::createWidgets()
{
  if (m_objectBaseName.isEmpty())
    return;
  foreach (PropertyWidgetTabFactoryBase *factory, s_tabFactories) {
    if (!factoryInUse(factory) && extensionAvailable(factory)) {
      const PageInfo pi = { factory, factory->createWidget(this) };
      m_pages.push_back(pi);
    }
  }

  std::sort(m_pages.begin(), m_pages.end(), [](const PageInfo &lhs, const PageInfo &rhs) {
      if (lhs.factory->priority() == rhs.factory->priority())
          return s_tabFactories.indexOf(lhs.factory) < s_tabFactories.indexOf(rhs.factory);
      return lhs.factory->priority() < rhs.factory->priority();
  });
}

void PropertyWidget::updateShownTabs()
{
  setUpdatesEnabled(false);
  createWidgets();

  auto prevSelectedWidget = currentWidget();

  int tabIt = 0;
  foreach (const auto &page, m_pages) {
      const int index = indexOf(page.widget);
      if (extensionAvailable(page.factory)) {
          if (index != tabIt)
              removeTab(index);
          insertTab(tabIt, page.widget, page.factory->label());
          ++tabIt;
      } else if (index != -1) {
          removeTab(index);
      }
    }

    // try to restore selection
    if (!prevSelectedWidget) // first time
        setCurrentIndex(0);
    if (indexOf(prevSelectedWidget) >= 0)
        setCurrentWidget(prevSelectedWidget);

    setUpdatesEnabled(true);
}

bool PropertyWidget::extensionAvailable(PropertyWidgetTabFactoryBase* factory) const
{
  return m_controller->availableExtensions().contains(m_objectBaseName + '.' + factory->name());
}

bool PropertyWidget::factoryInUse(PropertyWidgetTabFactoryBase* factory) const
{
    return std::find_if(m_pages.begin(), m_pages.end(), [factory](const PageInfo &pi) {
        return pi.factory == factory;
    }) != m_pages.end();
}
