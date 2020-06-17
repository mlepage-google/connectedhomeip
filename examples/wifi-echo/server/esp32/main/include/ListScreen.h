/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 * @file ListScreen.h
 *
 * Simple list screen.
 *
 */

#ifndef LIST_SCREEN_H
#define LIST_SCREEN_H

#include "Screen.h"
#include "ScreenManager.h"

#if CONFIG_HAVE_DISPLAY

#include <functional>
#include <string>
#include <tuple>
#include <vector>

class ListScreen : public Screen
{
public:
    class Model
    {
    public:
        virtual ~Model() = default;
        virtual std::string GetTitle() { return std::string(); }
        virtual int GetItemCount() { return 0; }
        virtual std::string GetItemText(int i) { return std::string(); }
        virtual void ItemAction(int i) {}
    };

private:
    Model * model  = nullptr;
    bool hasFocus  = false;
    int focusIndex = -1;

public:
    ListScreen(Model * model) : model(model) {}

    virtual ~ListScreen() { delete model; }

    virtual std::string GetTitle() { return model->GetTitle(); }

    virtual std::string GetButtonText(int id);

    virtual void Display();

    virtual bool IsFocusable() { return model->GetItemCount() > 0; }

    virtual void Focus(FocusType focus);

    virtual void Action() { model->ItemAction(focusIndex); }
};

class SimpleListModel : public ListScreen::Model
{
    std::string title;
    std::function<void(int)> action;
    std::vector<std::tuple<std::string, std::function<void()>>> items;

public:
    virtual std::string GetTitle() { return title; }
    virtual int GetItemCount() { return items.size(); }
    virtual std::string GetItemText(int i) { return std::get<0>(items[i]); }

    virtual void ItemAction(int i)
    {
        auto & action = std::get<1>(items[i]);
        if (action)
        {
            action();
        }
        else if (this->action)
        {
            this->action(i);
        }
    }

    // Builder interface.

    SimpleListModel * Title(std::string title)
    {
        this->title = std::move(title);
        return this;
    }

    SimpleListModel * Action(std::function<void(int)> action)
    {
        this->action = std::move(action);
        return this;
    }

    SimpleListModel * Item(std::string text)
    {
        items.emplace_back(std::move(text), std::move(std::function<void()>()));
        return this;
    }

    SimpleListModel * Item(std::string text, std::function<void()> action)
    {
        items.emplace_back(std::move(text), std::move(action));
        return this;
    }
};

#endif // CONFIG_HAVE_DISPLAY

#endif // LIST_SCREEN_H
