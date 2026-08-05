#pragma once

#include <memory>

class MessageProcessorEngine;

std::unique_ptr<MessageProcessorEngine> createDefaultMessageProcessorEngine();
