defmodule Tau5.MonacoEditor do
  def apply_event_on_did_change_model_content(code, changes) do
    Enum.reduce(changes, code, fn change, acc ->
      apply_content_change(acc, change)
    end)
  end

  defp apply_content_change(code, %{
         "range" => _range,
         "text" => text,
         "rangeLength" => 0,
         "rangeOffset" => range_offset
       }) do
    insert_at(code, text, range_offset)
  end

  defp apply_content_change(code, %{
         "range" => _range,
         "text" => text,
         "rangeLength" => range_length,
         "rangeOffset" => range_offset
       }) do
    replace_at(code, text, range_offset, range_length)
  end

  defp insert_at(original, insert, index) do
    replace_at(original, insert, index, 0)
  end

  defp replace_at(original, insert, index, replace_length) do
    part1 = String.slice(original, 0, index)
    part2 = String.slice(original, (index + replace_length)..-1)
    part1 <> insert <> part2
  end
end
