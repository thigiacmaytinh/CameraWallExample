using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml.Linq;

namespace WpfCameraWall
{
    public static class LayoutLoader
    {
        public static List<LayoutInfo> Load(string xmlPath)
        {
            if (!File.Exists(xmlPath))
                return new List<LayoutInfo>();

            XDocument doc = XDocument.Load(xmlPath);

            var result = new List<LayoutInfo>();

            foreach (XElement config in doc.Root!.Elements("ElementConfig"))
            {
                string name = ReadText(config, "Name");
                int type = ReadInt(config, "Type");
                int wndSize = ReadInt(config, "WndSize");

                string rowStretch = ReadText(config, "RowStretch");
                string columnStretch = ReadText(config, "ColumnStretch");

                int rows = CountStretch(rowStretch);
                int cols = CountStretch(columnStretch);

                bool isWidescreen = ReadText(config, "Widescreen") == "1";

                var cells = new List<LayoutCell>();

                XElement? items = config.Element("ElementItems");
                if (items != null)
                {
                    foreach (XElement item in items.Elements())
                    {
                        if (!string.Equals(item.Name.LocalName, "Element", StringComparison.OrdinalIgnoreCase))
                            continue;

                        string itemType = item.Attribute("type")?.Value ?? "";

                        // Ignore QSpacerItem. Only QWidget is a video block.
                        if (!string.Equals(itemType, "QWidget", StringComparison.OrdinalIgnoreCase))
                            continue;

                        cells.Add(new LayoutCell
                        {
                            CellId = ReadAttrInt(item, "id"),
                            X = ReadAttrInt(item, "column"),
                            Y = ReadAttrInt(item, "row"),
                            W = ReadAttrInt(item, "columnSpan", 1),
                            H = ReadAttrInt(item, "rowSpan", 1),
                            CameraId = -1
                        });
                    }
                }

                if (rows <= 0)
                    rows = cells.Count == 0 ? 1 : cells.Max(c => c.Y + c.H);

                if (cols <= 0)
                    cols = cells.Count == 0 ? 1 : cells.Max(c => c.X + c.W);

                var layout = new CameraLayout
                {
                    Name = name,
                    Rows = rows,
                    Cols = cols,
                    Cells = cells.OrderBy(c => c.CellId).ToList()
                };

                result.Add(new LayoutInfo
                {
                    Name = name,
                    Type = type,
                    WndSize = wndSize,
                    IsWidescreen = isWidescreen,
                    Layout = layout
                });
            }

            return result;
        }

        private static string ReadText(XElement parent, string name)
        {
            return parent.Element(name)?.Value?.Trim() ?? "";
        }

        private static int ReadInt(XElement parent, string name, int defaultValue = 0)
        {
            string text = ReadText(parent, name);
            return int.TryParse(text, out int value) ? value : defaultValue;
        }

        private static int ReadAttrInt(XElement element, string name, int defaultValue = 0)
        {
            string? text = element.Attribute(name)?.Value;
            return int.TryParse(text, out int value) ? value : defaultValue;
        }

        private static int CountStretch(string stretch)
        {
            if (string.IsNullOrWhiteSpace(stretch))
                return 0;

            return stretch.Split(',').Count(x => !string.IsNullOrWhiteSpace(x));
        }
    }
}