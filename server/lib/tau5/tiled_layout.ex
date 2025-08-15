defmodule Tau5.TiledLayout do
  @moduledoc """
  A tmux-like layout system for panels.
  
  Panels are arranged in a binary tree where leaves are panels and branches are splits.
  Users interact with panels by index (0, 1, 2...) which are assigned left-to-right.
  
  ## Examples
  
      iex> layout = SimpleLayout.new()
      iex> layout = SimpleLayout.split_horizontal(layout, 0)
      iex> SimpleLayout.panel_count(layout)
      2
      
  """

  defstruct tree: nil,
            index: %{},
            active: 0,
            zoom: nil

  @type panel_index :: non_neg_integer()
  @type panel_id :: String.t()
  @type direction :: :horizontal | :vertical
  @type ratio :: float()
  
  @type t :: %__MODULE__{
    tree: tree_node(),
    index: %{panel_index() => panel_id()},
    active: panel_index(),
    zoom: nil | panel_index()  # Just store which panel is zoomed
  }
  
  @type tree_node :: panel() | split()
  
  @type panel :: %{
    type: :panel,
    id: panel_id()
  }
  
  @type split :: %{
    type: :split,
    id: panel_id(),
    direction: direction(),
    ratio: ratio(),
    left: tree_node(),
    right: tree_node()
  }

  @doc """
  Creates a new layout with a single panel.
  """
  @spec new() :: t()
  def new do
    panel = new_panel()
    
    %__MODULE__{
      tree: panel,
      index: %{0 => panel.id},
      active: 0
    }
  end

  @doc """
  Splits the panel at the given index horizontally.
  """
  @spec split_horizontal(t(), panel_index()) :: t()
  def split_horizontal(layout, panel_index) when panel_index < 0 do
    # Invalid index, return unchanged
    layout
  end
  
  def split_horizontal(layout, panel_index) do
    do_split(layout, panel_index, :horizontal)
  end

  @doc """
  Splits the panel at the given index vertically.
  """
  @spec split_vertical(t(), panel_index()) :: t()
  def split_vertical(layout, panel_index) when panel_index < 0 do
    # Invalid index, return unchanged
    layout
  end
  
  def split_vertical(layout, panel_index) do
    do_split(layout, panel_index, :vertical)
  end

  @doc """
  Closes the panel at the given index.
  The sibling panel takes its place. Cannot close the last panel.
  """
  @spec close_panel(t(), panel_index()) :: t()
  def close_panel(layout, panel_index) when panel_index < 0 do
    # Invalid index, return unchanged
    layout
  end
  
  def close_panel(%{tree: %{type: :panel}} = layout, _panel_index) do
    # Cannot close the last panel
    layout
  end

  def close_panel(layout, panel_index) do
    with {:ok, panel_id} <- get_panel_id(layout, panel_index),
         {:ok, new_tree} <- remove_panel_from_tree(layout.tree, panel_id) do
      rebuild_state(layout, new_tree)
    else
      _ -> layout
    end
  end

  @doc """
  Updates the ratio of a split.
  Used by the UI when dragging splitters.
  """
  @spec update_ratio(t(), panel_id(), ratio()) :: t()
  def update_ratio(layout, split_id, new_ratio) do
    # Validate and clamp ratio
    clamped_ratio = clamp_ratio(new_ratio)
    new_tree = do_update_ratio(layout.tree, split_id, clamped_ratio)
    %{layout | tree: new_tree}
  end

  @doc """
  Applies an even horizontal layout to all panels.
  """
  @spec apply_even_horizontal(t()) :: t()
  def apply_even_horizontal(layout) do
    panels = collect_panels(layout.tree)
    new_tree = build_balanced_tree(panels, :horizontal)
    rebuild_state(layout, new_tree)
  end

  @doc """
  Applies a main-vertical layout (one main panel, others stacked).
  The active panel becomes the main panel.
  """
  @spec apply_main_vertical(t()) :: t()
  def apply_main_vertical(layout) do
    panels = collect_panels(layout.tree)
    
    case panels do
      [_single] -> 
        layout
      _ ->
        # Get the active panel and make it the main panel
        active_panel_id = Map.get(layout.index, layout.active)
        {main_panel, other_panels} = 
          case Enum.split_with(panels, fn p -> p.id == active_panel_id end) do
            {[main], others} -> {main, others}
            {[], panels} -> 
              # Fallback if active panel not found (shouldn't happen)
              [main | rest] = panels
              {main, rest}
          end
        
        new_tree = %{
          type: :split,
          id: generate_id(),
          direction: :horizontal,
          ratio: 0.7,
          left: main_panel,
          right: build_balanced_tree(other_panels, :vertical)
        }
        rebuild_state(layout, new_tree)
    end
  end

  @doc """
  Applies a tiled layout (balanced grid).
  """
  @spec apply_tiled(t()) :: t()
  def apply_tiled(layout) do
    panels = collect_panels(layout.tree)
    new_tree = build_tiled_tree(panels)
    rebuild_state(layout, new_tree)
  end

  @doc """
  Zooms a panel to fill the entire layout.
  Just marks which panel is zoomed - rendering will handle hiding others.
  """
  @spec zoom_panel(t(), panel_index()) :: t()
  def zoom_panel(layout, panel_index) when panel_index < 0 do
    # Invalid index, return unchanged
    layout
  end
  
  def zoom_panel(layout, panel_index) do
    if Map.has_key?(layout.index, panel_index) do
      %{layout | zoom: panel_index}
    else
      layout
    end
  end

  @doc """
  Restores the layout from zoom.
  """
  @spec unzoom(t()) :: t()
  def unzoom(layout) do
    %{layout | zoom: nil}
  end

  @doc """
  Swaps the positions of two panels.
  """
  @spec swap_panels(t(), panel_index(), panel_index()) :: t()
  def swap_panels(layout, index1, index2) when index1 < 0 or index2 < 0 do
    # Invalid indices, return unchanged
    layout
  end
  
  def swap_panels(layout, index1, index2) when index1 == index2 do
    # Same index, no-op
    layout
  end
  
  def swap_panels(layout, index1, index2) do
    with {:ok, id1} <- get_panel_id(layout, index1),
         {:ok, id2} <- get_panel_id(layout, index2),
         true <- id1 != id2 do
      new_tree = 
        layout.tree
        |> swap_node_id(id1, :temp)
        |> swap_node_id(id2, id1)
        |> swap_node_id(:temp, id2)
      
      rebuild_state(layout, new_tree)
    else
      _ -> layout
    end
  end

  @doc """
  Sets the active (focused) panel.
  """
  @spec focus_panel(t(), panel_index()) :: t()
  def focus_panel(layout, panel_index) when panel_index < 0 do
    # Invalid index, return unchanged
    layout
  end
  
  def focus_panel(layout, panel_index) do
    if Map.has_key?(layout.index, panel_index) do
      %{layout | active: panel_index}
    else
      layout
    end
  end

  @doc """
  Returns the number of panels in the layout.
  """
  @spec panel_count(t()) :: non_neg_integer()
  def panel_count(layout) do
    map_size(layout.index)
  end

  # Private functions

  defp do_split(layout, panel_index, direction) do
    with {:ok, panel_id} <- get_panel_id(layout, panel_index) do
      new_split = %{
        type: :split,
        id: generate_id(),
        direction: direction,
        ratio: 0.5,
        left: new_panel(panel_id),
        right: new_panel()
      }
      
      new_tree = replace_node(layout.tree, panel_id, new_split)
      rebuild_state(layout, new_tree)
    else
      _ -> layout
    end
  end

  defp get_panel_id(layout, panel_index) do
    case Map.fetch(layout.index, panel_index) do
      {:ok, id} -> {:ok, id}
      :error -> :error
    end
  end

  defp rebuild_state(layout, new_tree) do
    new_index = build_index(new_tree)
    new_active = min(layout.active, map_size(new_index) - 1)
    
    %{layout |
      tree: new_tree,
      index: new_index,
      active: new_active
    }
  end

  defp build_index(tree) do
    tree
    |> collect_panels()
    |> Enum.with_index()
    |> Map.new(fn {panel, idx} -> {idx, panel.id} end)
  end

  defp collect_panels(%{type: :panel} = panel), do: [panel]
  defp collect_panels(%{type: :split, left: left, right: right}) do
    collect_panels(left) ++ collect_panels(right)
  end

  defp replace_node(%{type: :panel, id: target_id}, target_id, replacement), do: replacement
  defp replace_node(%{type: :panel} = node, _target_id, _replacement), do: node
  defp replace_node(%{type: :split, id: target_id}, target_id, replacement), do: replacement
  defp replace_node(%{type: :split} = split, target_id, replacement) do
    %{split |
      left: replace_node(split.left, target_id, replacement),
      right: replace_node(split.right, target_id, replacement)
    }
  end

  defp do_update_ratio(%{type: :split, id: target_id} = split, target_id, new_ratio) do
    %{split | ratio: new_ratio}
  end
  defp do_update_ratio(%{type: :split} = split, target_id, new_ratio) do
    %{split |
      left: do_update_ratio(split.left, target_id, new_ratio),
      right: do_update_ratio(split.right, target_id, new_ratio)
    }
  end
  defp do_update_ratio(node, _target_id, _new_ratio), do: node

  defp swap_node_id(%{type: :panel, id: target_id} = node, target_id, new_id) do
    %{node | id: new_id}
  end
  defp swap_node_id(%{type: :panel} = node, _target_id, _new_id), do: node
  defp swap_node_id(%{type: :split} = split, target_id, new_id) do
    %{split |
      left: swap_node_id(split.left, target_id, new_id),
      right: swap_node_id(split.right, target_id, new_id)
    }
  end

  defp remove_panel_from_tree(tree, panel_id) do
    do_remove_panel(tree, panel_id, nil)
  end

  defp do_remove_panel(%{type: :panel}, _panel_id, _parent_id), do: :error
  defp do_remove_panel(%{type: :split} = split, panel_id, _parent_id) do
    cond do
      match?(%{type: :panel, id: ^panel_id}, split.left) ->
        {:ok, split.right}
      match?(%{type: :panel, id: ^panel_id}, split.right) ->
        {:ok, split.left}
      true ->
        with {:ok, new_left} <- do_remove_panel(split.left, panel_id, split.id) do
          {:ok, %{split | left: new_left}}
        else
          _ ->
            case do_remove_panel(split.right, panel_id, split.id) do
              {:ok, new_right} -> {:ok, %{split | right: new_right}}
              _ -> :error
            end
        end
    end
  end

  defp build_balanced_tree([single], _direction), do: single
  defp build_balanced_tree(panels, direction) do
    {left_panels, right_panels} = Enum.split(panels, div(length(panels), 2))
    next_direction = toggle_direction(direction)
    
    %{
      type: :split,
      id: generate_id(),
      direction: direction,
      ratio: 0.5,
      left: build_balanced_tree(left_panels, next_direction),
      right: build_balanced_tree(right_panels, next_direction)
    }
  end

  defp build_tiled_tree(panels) do
    columns = :math.sqrt(length(panels)) |> ceil()
    build_grid_tree(panels, columns)
  end

  defp build_grid_tree([single], _columns), do: single
  defp build_grid_tree(panels, columns) when length(panels) <= columns do
    build_balanced_tree(panels, :horizontal)
  end
  defp build_grid_tree(panels, columns) do
    rows = Enum.chunk_every(panels, columns)
    
    rows
    |> Enum.map(&build_balanced_tree(&1, :horizontal))
    |> build_balanced_tree(:vertical)
  end

  defp toggle_direction(:horizontal), do: :vertical
  defp toggle_direction(:vertical), do: :horizontal

  defp new_panel(id \\ nil) do
    %{
      type: :panel,
      id: id || generate_id()
    }
  end

  defp clamp_ratio(ratio) when is_number(ratio) do
    ratio
    |> max(0.2)
    |> min(0.8)
  end
  
  defp clamp_ratio(_) do
    # Default to center if invalid ratio
    0.5
  end

  defp generate_id do
    :crypto.strong_rand_bytes(8) 
    |> Base.encode16(case: :lower)
  end
end