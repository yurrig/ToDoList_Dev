﻿namespace MSDN.Html.Editor
{
	partial class TableRowPropertyForm
	{
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows Form Designer generated code

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(TableRowPropertyForm));
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.hAlignComboBox = new System.Windows.Forms.ComboBox();
			this.vAlignComboBox = new System.Windows.Forms.ComboBox();
			this.cancel = new System.Windows.Forms.Button();
			this.ok = new System.Windows.Forms.Button();
			this.groupBox1 = new System.Windows.Forms.GroupBox();
			this.checkBackColor = new System.Windows.Forms.CheckBox();
			this.btnBackColor = new MSDN.Html.Editor.Classes.ColorButton();
			this.groupBox1.SuspendLayout();
			this.SuspendLayout();
			// 
			// label1
			// 
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(13, 20);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(102, 13);
			this.label1.TabIndex = 0;
			this.label1.Text = "Horizontal alignment";
			// 
			// label2
			// 
			this.label2.AutoSize = true;
			this.label2.Location = new System.Drawing.Point(13, 50);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(90, 13);
			this.label2.TabIndex = 0;
			this.label2.Text = "Vertical alignment";
			// 
			// hAlignComboBox
			// 
			this.hAlignComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.hAlignComboBox.FormattingEnabled = true;
			this.hAlignComboBox.Location = new System.Drawing.Point(157, 17);
			this.hAlignComboBox.Name = "hAlignComboBox";
			this.hAlignComboBox.Size = new System.Drawing.Size(104, 21);
			this.hAlignComboBox.TabIndex = 1;
			// 
			// vAlignComboBox
			// 
			this.vAlignComboBox.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.vAlignComboBox.FormattingEnabled = true;
			this.vAlignComboBox.Location = new System.Drawing.Point(157, 47);
			this.vAlignComboBox.Name = "vAlignComboBox";
			this.vAlignComboBox.Size = new System.Drawing.Size(104, 21);
			this.vAlignComboBox.TabIndex = 1;
			// 
			// cancel
			// 
			this.cancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.cancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.cancel.Location = new System.Drawing.Point(202, 120);
			this.cancel.Name = "cancel";
			this.cancel.Size = new System.Drawing.Size(75, 23);
			this.cancel.TabIndex = 4;
			this.cancel.Text = "Cancel";
			this.cancel.UseVisualStyleBackColor = true;
			// 
			// ok
			// 
			this.ok.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ok.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.ok.Location = new System.Drawing.Point(121, 120);
			this.ok.Name = "ok";
			this.ok.Size = new System.Drawing.Size(75, 23);
			this.ok.TabIndex = 5;
			this.ok.Text = "OK";
			this.ok.UseVisualStyleBackColor = true;
			// 
			// groupBox1
			// 
			this.groupBox1.Controls.Add(this.checkBackColor);
			this.groupBox1.Controls.Add(this.btnBackColor);
			this.groupBox1.Controls.Add(this.vAlignComboBox);
			this.groupBox1.Controls.Add(this.hAlignComboBox);
			this.groupBox1.Controls.Add(this.label2);
			this.groupBox1.Controls.Add(this.label1);
			this.groupBox1.Location = new System.Drawing.Point(6, 3);
			this.groupBox1.Name = "groupBox1";
			this.groupBox1.Size = new System.Drawing.Size(271, 109);
			this.groupBox1.TabIndex = 6;
			this.groupBox1.TabStop = false;
			// 
			// checkBackColor
			// 
			this.checkBackColor.AutoSize = true;
			this.checkBackColor.Location = new System.Drawing.Point(16, 79);
			this.checkBackColor.Name = "checkBackColor";
			this.checkBackColor.Size = new System.Drawing.Size(116, 17);
			this.checkBackColor.TabIndex = 3;
			this.checkBackColor.Text = "Background colour";
			this.checkBackColor.UseVisualStyleBackColor = true;
			this.checkBackColor.CheckedChanged += new System.EventHandler(this.checkBackColor_CheckedChanged);
			// 
			// btnBackColor
			// 
			this.btnBackColor.Color = System.Drawing.Color.Empty;
			this.btnBackColor.Location = new System.Drawing.Point(156, 75);
			this.btnBackColor.Name = "btnBackColor";
			this.btnBackColor.Size = new System.Drawing.Size(75, 22);
			this.btnBackColor.TabIndex = 2;
			this.btnBackColor.Text = "Set...";
			this.btnBackColor.TextAlign = System.Drawing.ContentAlignment.MiddleRight;
			this.btnBackColor.UseVisualStyleBackColor = true;
			// 
			// TableRowPropertyForm
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.cancel;
			this.ClientSize = new System.Drawing.Size(284, 151);
			this.Controls.Add(this.groupBox1);
			this.Controls.Add(this.ok);
			this.Controls.Add(this.cancel);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedDialog;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "TableRowPropertyForm";
			this.ShowInTaskbar = false;
			this.Text = "Row Properties";
			this.groupBox1.ResumeLayout(false);
			this.groupBox1.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.ComboBox hAlignComboBox;
		private System.Windows.Forms.ComboBox vAlignComboBox;
		private Classes.ColorButton btnBackColor;
		private System.Windows.Forms.Button cancel;
		private System.Windows.Forms.Button ok;
		private System.Windows.Forms.GroupBox groupBox1;
		private System.Windows.Forms.CheckBox checkBackColor;
	}
}