//============================================================================
// Distributed under the Apache License, Version 2.0.
// Author: Raphael Menges (raphaelmenges@uni-koblenz.de)
//============================================================================

#include "SelectFieldOptionsAction.h"
#include "src/State/Web/Tab/Interface/TabInteractionInterface.h"
#include "submodules/eyeGUI/include/eyeGUI.h"

SelectFieldOptionsAction::SelectFieldOptionsAction(TabInteractionInterface *pTab) : Action(pTab)
{
    // TODO
}

SelectFieldOptionsAction::~SelectFieldOptionsAction()
{
    // Nothing to do
}

bool SelectFieldOptionsAction::Update(float tpf, TabInput tabInput)
{
    // TODO

    // Action is done
    return true;
}

void SelectFieldOptionsAction::Draw() const
{
    // Nothing to do
}

void SelectFieldOptionsAction::Activate()
{
    // Nothing to do
}

void SelectFieldOptionsAction::Deactivate()
{
    // Nothing to do
}

void SelectFieldOptionsAction::Abort()
{
    // Nothing to do
}