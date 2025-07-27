defmodule Tau5.Layout do
  @moduledoc """
  Manages the layout tree structure for split panes in the UI.
  Each widget is a simple color display.
  """

  @type orientation :: :horizontal | :vertical
  
  @type t :: leaf() | split()
  
  @type leaf :: %{
    id: String.t(),
    type: :leaf,
    opts: map()  # Contains color and any other widget options
  }
  
  @type split :: %{
    id: String.t(),
    type: :split,
    orientation: orientation(),
    ratio: float(),
    left: t(),
    right: t()
  }

  @doc """
  Creates a new leaf node (widget) with options.
  Automatically assigns a random color if not provided.
  """
  def new_leaf(opts \\ %{}) do
    %{
      id: generate_id(),
      type: :leaf,
      opts: Map.put_new(opts, :color, random_color())
    }
  end

  @doc """
  Creates a new split node with two children.
  """
  def new_split(orientation, left, right, ratio \\ 0.5) do
    %{
      id: generate_id(),
      type: :split,
      orientation: orientation,
      ratio: clamp_ratio(ratio),
      left: left,
      right: right
    }
  end

  @doc """
  Updates the ratio of a split node with the given ID.
  Returns the updated tree.
  """
  def update_ratio(node, target_id, new_ratio) do
    case node do
      %{id: ^target_id, type: :split} = split ->
        %{split | ratio: clamp_ratio(new_ratio)}
      
      %{type: :split, left: left, right: right} = split ->
        %{split | 
          left: update_ratio(left, target_id, new_ratio),
          right: update_ratio(right, target_id, new_ratio)
        }
      
      leaf ->
        leaf
    end
  end

  @doc """
  Splits a leaf node into two panes.
  Returns {:ok, updated_tree} or {:error, :not_found}
  """
  def split_node(node, target_id, orientation, new_leaf_opts \\ %{}) do
    case find_and_replace(node, target_id, fn leaf ->
      new_leaf = new_leaf(new_leaf_opts)
      new_split(orientation, leaf, new_leaf)
    end) do
      {:ok, updated} -> {:ok, updated}
      :not_found -> {:error, :not_found}
    end
  end

  @doc """
  Finds a node by ID in the tree.
  """
  def find_node(node, target_id) do
    case node do
      %{id: ^target_id} -> {:ok, node}
      
      %{type: :split, left: left, right: right} ->
        case find_node(left, target_id) do
          {:ok, found} -> {:ok, found}
          :not_found -> find_node(right, target_id)
        end
      
      _ -> :not_found
    end
  end

  @doc """
  Removes a split, promoting one of its children to take its place.
  Returns {:ok, updated_tree} or {:error, :not_found}
  """
  def remove_split(node, split_id, keep \\ :left) do
    case node do
      %{id: ^split_id, type: :split, left: left, right: right} ->
        {:ok, if(keep == :left, do: left, else: right)}
      
      %{type: :split, left: left, right: right} = split ->
        case remove_split(left, split_id, keep) do
          {:ok, updated} -> {:ok, %{split | left: updated}}
          :not_found ->
            case remove_split(right, split_id, keep) do
              {:ok, updated} -> {:ok, %{split | right: updated}}
              :not_found -> {:error, :not_found}
            end
        end
      
      _ -> {:error, :not_found}
    end
  end

  # Private functions

  defp find_and_replace(node, target_id, replace_fn) do
    case node do
      %{id: ^target_id, type: :leaf} = leaf ->
        {:ok, replace_fn.(leaf)}
      
      %{type: :split, left: left, right: right} = split ->
        case find_and_replace(left, target_id, replace_fn) do
          {:ok, updated} -> 
            {:ok, %{split | left: updated}}
          :not_found ->
            case find_and_replace(right, target_id, replace_fn) do
              {:ok, updated} -> 
                {:ok, %{split | right: updated}}
              :not_found -> 
                :not_found
            end
        end
      
      _ -> :not_found
    end
  end

  defp clamp_ratio(ratio) do
    # On mobile, we need larger minimum ratios to ensure usability
    # This prevents panes from becoming too small to interact with
    ratio
    |> max(0.2)  # 20% minimum ensures at least 60px on a 300px screen
    |> min(0.8)  # 80% maximum ensures the other pane gets at least 20%
  end

  defp generate_id do
    "layout_#{:crypto.strong_rand_bytes(4) |> Base.encode16(case: :lower)}"
  end

  defp random_color do
    # Generate a pleasant random color using HSL
    hue = :rand.uniform(360)
    "hsl(#{hue}, 70%, 60%)"
  end
end