file(READ "${vienna_game_ai_SOURCE_DIR}/include/ViennaGameAILibrary.hpp" VIENNA_CONTENT)

function(patch_vienna_fragment original replacement)
    string(FIND "${VIENNA_CONTENT}" "${original}" fragment_position)
    if(fragment_position EQUAL -1)
        message(FATAL_ERROR "Pinned Vienna Game AI source changed: ${original}")
    endif()
    string(REPLACE "${original}" "${replacement}" VIENNA_CONTENT "${VIENNA_CONTENT}")
    set(VIENNA_CONTENT "${VIENNA_CONTENT}" PARENT_SCOPE)
endfunction()

patch_vienna_fragment("~NavMesh() {}" "~NavMesh() { delete m_regions; }")
patch_vienna_fragment("if (u == 0 && v == 0)" "if ((u == 0 && v == 0) || (u != 0 && v != 0))")
patch_vienna_fragment("auto val = randomFloat(0.0f, 100.0f);"
    "auto val = obstaclePercentage == 0.0f ? 0.0f : randomFloat(0.0f, 100.0f);")
patch_vienna_fragment("std::vector<Vec2ui> findPath(Vec2ui start, Vec2ui target)"
    "std::function<bool(Vec2ui, Vec2ui)> canTraverse;\n        std::vector<Vec2ui> findPath(Vec2ui start, Vec2ui target)")
patch_vienna_fragment("if (neighbor.state == NodeState::OBSTRUCTABLE)"
    "if (neighbor.state == NodeState::OBSTRUCTABLE || (canTraverse && !canTraverse(nodes[currentIndex].pos, neighbor.pos)))")

set(VIENNA_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/vienna-game-ai/include")
file(MAKE_DIRECTORY "${VIENNA_INCLUDE_DIR}")
file(WRITE "${VIENNA_INCLUDE_DIR}/ViennaGameAILibrary.hpp" "${VIENNA_CONTENT}")