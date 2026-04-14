# 🎮 World Ruin: Complete Controls & Command Reference

Welcome to the World Ruin engine! This guide covers every mechanic, keybind, and systemic interaction currently available in the game, structured from basic camera controls to advanced automated logistics.

---

## 🟢 TIER 1: The Basics (Camera & View)

Mastering your view of the world is the first step to managing your ruins.

* **W, A, S, D (Free-Look):** Pains the camera across the world map.
* *Note:* This is disabled when a single unit is selected, as the camera automatically tracks them.


* **C / Z (Orbital Rotation):** Rotates the 3D world view 90° clockwise (C) or counter-clockwise (Z). Sprites and shadows dynamically adjust to the new perspective.
* **Mouse Wheel:** Zoom in for a detailed view of the action or zoom out for a high-level strategic LOD (Level of Detail) view.

---

## 🟡 TIER 2: Basic Command (Selection & Movement)

Selecting your units and moving them around the grid.

* **Left Click (Single Unit):** Selects one unit. This activates **Hero Mode**, locking the camera to the unit and enabling manual WASD movement.
* **Left Click Drag (Blue Box):** Draws a bounding box in 3D world-space. If units are inside, it selects them.
* **Right Click (Ground):** Issues a move command to the targeted point. Multiple selected units will automatically calculate an A* path and walk in a non-overlapping grid formation.
* **Left Click Drag (On Empty Ground):** If you already have units selected, dragging a box on empty ground acts as a **Formation Move**. Units will spread out to fill the box you just drew.
* **ESC:** Clears your current unit selection.
* *Note:* Deselecting units **does not** cancel their active tasks.



---

## 🟠 TIER 3: Direct Control (Hero Mode)

When exactly **one unit** is selected, you take direct control over them.

* **W, A, S, D (Hero Movement):** Directly walk the unit around the world.
* *Note:* Manually walking a unit away from a task will automatically force them to **abandon that task** so others can take over.


* **Spacebar (Attack / Interact):** Swings the unit's weapon in a frontal arc.
* **Holding Space:** Warriors, Lumberjacks, and Miners can hold down the spacebar to continuously attack/mine resources in front of them.
* **Single Press (Couriers Only):** Couriers require a single, distinct press of the spacebar to manually Pick Up or Drop a log/small rock. This prevents them from instantly re-picking up an item they just dropped.



---

## 🔴 TIER 4: Advanced Selection & Unit Roles

To effectively manage a settlement, you must understand unit roles and strict selection.

### Strict Selection

* **Ctrl + Left Click Drag (Golden Box):** A high-priority selection box. Unlike a normal drag (which might trigger a formation move if units are already selected), Ctrl+Drag *strictly* forces a new selection, ignoring all other context.

### Unit Roles & Capabilities

* ⚔️ **Warrior:** Can attack enemies and clear **Bushes**.
* 🪓 **Lumberjack:** Can chop down **Trees** and clear **Bushes**.
* ⛏️ **Miner:** Can break down **Large Rocks** (Rock Types 2 & 4).
* 🎒 **Courier:** Has no attack damage. Designed purely to pick up **Logs** and **Small Rocks** (Rock Types 1 & 3) and carry them to drop-off zones.

---

## 🟣 TIER 5: The Task System (Automated Harvesting)

Instead of micromanaging, you can draw zones and let the engine assign workers.

* **Q + Left Click Drag (White Box):** Enters **Pending Task Mode**. Draws a highlighted harvest zone on the terrain.
* **1, 2, 3, 4, 5 (Filter Resources):** While the white Pending Task box is active, you can toggle exactly what you want your workers to harvest:
* `1`: Toggle **Trees**
* `2`: Toggle **Large Rocks**
* `3`: Toggle **Bushes**
* `4`: Toggle **Logs** (For Couriers)
* `5`: Toggle **Small Rocks** (For Couriers)


* **Enter:** Confirms the task. A Task ID is generated, a colored transparent overlay appears, and floating UI shows exactly how many units of each type are assigned to the zone.

---

## ⚫ TIER 6: Logistics & Drop-offs (Couriers)

Resources on the ground are useless unless moved to a stockpile.

* **Specify Drop-off Zone (Q in Pending Mode):** While drawing a Pending Task (White Box), press **Q** again. The box will turn **Green**. Dragging now specifies where Couriers should take the logs/rocks they collect from the harvest zone.
* **Instant Pure Drop-off (Q + Drag):** If you *only* have loaded Couriers selected (Couriers currently carrying wood or rocks), pressing Q+Drag instantly creates a drop-off zone and forces them to unpack their cargo in a neat, mathematically aligned grid pattern.
* **Auto-Sorting:** Couriers dropping items into a drop-off zone will not drop them randomly. They read how many items are already in the zone and place the new item into a perfect sub-grid structure.

---

## 🟤 TIER 7: Master Level (Future Drops & Task Cleanup)

The most advanced mechanics for fully autonomous supply chains and workflow management.

### Future Drops (Auto-Collect)

* **F (Toggle Auto-Collect):** While in Pending Task Mode (with at least one Courier selected), pressing **F** activates "Future Drops".
* *How it works:* Normally, Couriers only pick up logs/rocks that exist *when the task is created*. With Future Drops **[ON]**, the tree/large rock retains its task memory. When a Lumberjack fells a tree or a Miner breaks a boulder, the resulting explosion of logs/small rocks will instantly inherit the task. Couriers will patiently wait, watch the debris bounce, and immediately run to catch the newly spawned items.



### Safe Task Cancellation

* **Ctrl + Q (Contextual Cancel):** 1. If you are in Pending Task Mode and currently drawing a Drop-off zone, `Ctrl + Q` cancels *only* the drop-off selection, returning you to the Harvest selection.
2. If pressed again, it cancels the Pending Task entirely.
3. If pressed while units are actively working, it forces the selected units to drop their items, clear their paths, and un-claim their targets.
* **Automatic Task Abandonment:** If an active Task Zone has no resources left to harvest/move, AND no active Couriers are currently walking towards it with cargo, the Task Zone successfully concludes and gracefully deletes itself from the UI.